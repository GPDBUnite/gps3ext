#include "S3Common.h"

#include "utils.h"

#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <iostream>
#include <sstream>
#include <map>

using std::stringstream;

bool SignGETv2(HeaderContent *h, const char *path, const S3Credential &cred) {
    char timestr[64];

    // CONTENT_LENGTH is not a part of StringToSign
    h->Add(CONTENTLENGTH, "0");

    gethttpnow(timestr);
    h->Add(DATE, timestr);
    stringstream sstr;
    sstr << "GET\n\n\n" << timestr << "\n" << path;
    char *tmpbuf = sha1hmac(sstr.str().c_str(), cred.secret.c_str());
    int len = strlen(tmpbuf);
    char *signature = Base64Encode(tmpbuf, len);
    sstr.clear();
    sstr.str("");
    sstr << "AWS " << cred.keyid << ":" << signature;
    free(signature);
    h->Add(AUTHORIZATION, sstr.str().c_str());

    return true;
}

bool SignPUTv2(HeaderContent *h, const char *path, const S3Credential &cred) {
    char timestr[64];
    string typestr;

    gethttpnow(timestr);
    h->Add(DATE, timestr);
    stringstream sstr;

    typestr = h->Get(CONTENTTYPE);

    sstr << "PUT\n\n" << typestr << "\n" << timestr << "\n" << path;
    char *tmpbuf = sha1hmac(sstr.str().c_str(), cred.secret.c_str());
    int len = strlen(tmpbuf);
    char *signature = Base64Encode(tmpbuf, len);
    sstr.clear();
    sstr.str("");
    sstr << "AWS " << cred.keyid << ":" << signature;
    free(signature);
    h->Add(AUTHORIZATION, sstr.str().c_str());

    return true;
}

bool SignPOSTv2(HeaderContent *h, const char *path, const S3Credential &cred) {
    char timestr[64];
    string md5str;
    string typestr;

    gethttpnow(timestr);
    h->Add(DATE, timestr);
    stringstream sstr;
    md5str = h->Get(CONTENTMD5);
    typestr = h->Get(CONTENTTYPE);

    sstr << "POST\n" << md5str << "\n" << typestr << "\n" << timestr << "\n"
         << path;
    char *tmpbuf = sha1hmac(sstr.str().c_str(), cred.secret.c_str());
    int len = strlen(tmpbuf);
    char *signature = Base64Encode(tmpbuf, len);
    sstr.clear();
    sstr.str("");
    sstr << "AWS " << cred.keyid << ":" << signature;
    free(signature);
    h->Add(AUTHORIZATION, sstr.str().c_str());

    return true;
}

bool SignLISTv2(HeaderContent *h, const char *path, const S3Credential &cred) {
    return SignGETv2(h, path, cred);
}

bool SignFETCHv2(HeaderContent *h, const char *path, const S3Credential &cred) {
    int len = strlen(path);
    char path_with_acl[len + 4];

    strncpy(path_with_acl, path, len);

    return SignGETv2(h, strncat(path_with_acl, "?acl", 4), cred);
}

bool SignDELETEv2(HeaderContent *h, const char *path,
                  const S3Credential &cred) {
    char timestr[64];

    // CONTENT_LENGTH is not a part of StringToSign
    h->Add(CONTENTLENGTH, "0");

    gethttpnow(timestr);
    h->Add(DATE, timestr);
    stringstream sstr;
    sstr << "DELETE\n\n\n" << timestr << "\n" << path;
    char *tmpbuf = sha1hmac(sstr.str().c_str(), cred.secret.c_str());
    int len = strlen(tmpbuf);
    char *signature = Base64Encode(tmpbuf, len);
    sstr.clear();
    sstr.str("");
    sstr << "AWS " << cred.keyid << ":" << signature;
    free(signature);
    h->Add(AUTHORIZATION, sstr.str().c_str());

    return true;
}

const char *GetFieldString(HeaderField f) {
    switch (f) {
        case HOST:
            return "Host";
        case RANGE:
            return "Range";
        case DATE:
            return "Date";
        case CONTENTLENGTH:
            return "Content-Length";
        case CONTENTMD5:
            return "Content-MD5";
        case CONTENTTYPE:
            return "Content-Type";
        case EXPECT:
            return "Expect";
        case AUTHORIZATION:
            return "Authorization";
        default:
            return "unknown";
    }
}

bool HeaderContent::Add(HeaderField f, const std::string &v) {
    if (!v.empty()) {
        this->fields[f] = std::string(v);
        return true;
    } else {
        return false;
    }
}

const char *HeaderContent::Get(HeaderField f) {
    const char *ret = NULL;
    if (!this->fields[f].empty()) {
        ret = this->fields[f].c_str();
    }
    return ret;
}

struct curl_slist *HeaderContent::GetList() {
    struct curl_slist *chunk = NULL;
    std::map<HeaderField, std::string>::iterator it;
    for (it = this->fields.begin(); it != this->fields.end(); it++) {
        std::stringstream sstr;
        sstr << GetFieldString(it->first) << ": " << it->second;
        chunk = curl_slist_append(chunk, sstr.str().c_str());
    }
    return chunk;
}

UrlParser::UrlParser(const char *url) {
    if (!url) {
        // throw exception
        return;
    }
    struct http_parser_url u;
    int len, result;
    len = strlen(url);
    this->fullurl = (char *)malloc(len + 1);
    sprintf(this->fullurl, "%s", url);
    result = http_parser_parse_url(this->fullurl, len, false, &u);
    if (result != 0) {
        EXTLOG("Parse error : %d\n", result);
        return;
    }
    // std::cout<<u.field_set<<std::endl;
    this->host = extract_field(&u, UF_HOST);
    this->schema = extract_field(&u, UF_SCHEMA);
    this->path = extract_field(&u, UF_PATH);
}

UrlParser::~UrlParser() {
    if (host) free(host);
    if (schema) free(schema);
    if (path) free(path);
    if (fullurl) free(fullurl);
}

char *UrlParser::extract_field(const struct http_parser_url *u,
                               http_parser_url_fields i) {
    char *ret = NULL;
    if ((u->field_set & (1 << i)) != 0) {
        ret = (char *)malloc(u->field_data[i].len + 1);
        if (ret) {
            memcpy(ret, this->fullurl + u->field_data[i].off,
                   u->field_data[i].len);
            ret[u->field_data[i].len] = 0;
        }
    }
    return ret;
}
