#include <cassert>
#include <vector>
#include <string>
#include <stdexcept>
#include <exception>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "logic/Crypto.h"

Crypto::Crypto()
{
    ERR_load_crypto_strings();
    int result = EVP_add_cipher(EVP_aes_256_xts());
    if(result != 1)
      throw std::runtime_error("Could not add AES256 XTS cipher.");
    OPENSSL_config(NULL);
}

std::vector<char> Crypto::hashPassword(const std::string &password)
{
    unsigned int bytesHashed = 0;
    std::vector<char> hash(512);
    EVP_MD_CTX * context = EVP_MD_CTX_create();
    try
    {
        if(!context)
            throw std::runtime_error("OpenSSL: could not create digest context.");
        if(EVP_DigestInit_ex(context, EVP_sha512(), NULL)!=1)
            throw std::runtime_error("OpenSSL: could not initialize digest.");
        if(EVP_DigestUpdate(context, (unsigned char*)password.c_str(), password.size()) != 1)
            throw std::runtime_error("OpenSSL: hashing failed.");

        if(EVP_DigestFinal_ex(context, (unsigned char*)hash.data(), &bytesHashed) != 1)
            throw std::runtime_error("OpenSSL: final hashing failed.");
        assert(bytesHashed == 64);

    }catch(std::exception &e)
    {
       EVP_MD_CTX_destroy(context);
       throw e;
    }

    EVP_MD_CTX_destroy(context);
    return hash;
}

static std::vector<char> makeIV(uint64_t sector)
{
    std::vector<char> result(16,'\0');
    for(int i = 0; i<16; i++)
    {
        result[i] = 0xFF & sector;
        sector = sector >> 8;
    }
    return result;
}

std::vector<char> Crypto::decryptMessage(const std::vector<char> &ciphertext,
                       const std::string &password,
                       uint64_t iv)
{
    if(ciphertext.size() % 128 != 0)
        throw std::invalid_argument("Ciphertext length not multiple of 128");

    EVP_CIPHER_CTX *ctx;

    if(!(ctx = EVP_CIPHER_CTX_new()))
         std::runtime_error("OpenSSL: could not create cipher context.");

    try
    {
        std::vector<char> plaintext(ciphertext.size());
        std::vector<char> key = hashPassword(password);
        std::vector<char> initVector  = makeIV(iv);
        int len;
        // Datos de longitud no mod 128 deberian fallar.
        EVP_CIPHER_CTX_set_padding(ctx,0);

        if(1 != EVP_DecryptInit_ex(ctx,
                                   EVP_aes_256_xts(),
                                   NULL,
                                   (unsigned char *)key.data(),
                                   (unsigned char *)initVector.data()))
          std::runtime_error("OpenSSL: could not initialize cipher.");

        if(1 != EVP_DecryptUpdate(ctx,
                                 (unsigned char*)plaintext.data(),
                                 &len,
                                 (const unsigned char*)ciphertext.data(),
                                 ciphertext.size()))
          std::runtime_error("OpenSSL: decryption failed.");


        if(1 != EVP_DecryptFinal_ex(ctx, (unsigned char*)plaintext.data() + len, &len))
            std::runtime_error("OpenSSL: final decryption failed.");
        assert(len == 0);
        return plaintext;
    }catch(std::exception &e){
        EVP_CIPHER_CTX_free(ctx);
        throw e;
    }
}

Crypto::~Crypto()
{
    EVP_cleanup();
    ERR_free_strings();
}
