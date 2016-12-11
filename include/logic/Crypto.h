#ifndef CRYPTO_H
#define CRYPTO_H
#include <cinttypes>
#include <vector>
#include <string>

/*
 * Clase que hace de interfaz entre nuestro programa y la API de OpenSSl. Para
 * no tener dando vuelta varias llamadas de inicialización y cleanup de OpenSSL.
 */
class Crypto
{
public:
    Crypto();
    ~Crypto();
    std::vector<char> hashPassword(const std::string &password);
    std::vector<char> decryptMessage(const std::vector<char> &ciphertext,
                        const std::string &password,
                        uint64_t iv = 0);
};

#endif // CRYPTO_H
