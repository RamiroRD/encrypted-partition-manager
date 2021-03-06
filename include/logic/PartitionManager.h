#ifndef PARTITION_MANAGER_H
#define PARTITION_MANAGER_H

#include <sys/types.h>
#include <sys/stat.h>

#include <stdexcept>
#include <string>
#include <atomic>
#include <mutex>
#include <list>
#include <cstdint>

#include <logic/Crypto.h>



struct CommandError : public std::runtime_error
{
    CommandError(const std::string &cmd) : std::runtime_error(cmd){};
    CommandError(const char *cmd) : std::runtime_error(cmd){};
};

struct SysCallError : public std::exception
{
    
    SysCallError(const char * syscall, int returnCode = -1)
    {
        mWhat += "Syscall: \"";
        mWhat += syscall;
        mWhat += "\". Return code: ";
        mWhat += std::to_string(returnCode);
    }
    SysCallError(const std::string &syscall, int returnCode = -1)
    {
        SysCallError(syscall.c_str(),returnCode);
    }

    const char * what() const noexcept
    {
        return mWhat.c_str();
    }

    const std::string &sysCall()
    {
        return mSysCall;
    }

    int returnCode()
    {
        return mReturnCode;
    }

    private:
    std::string mWhat;
    std::string mSysCall;
    int         mReturnCode;
};
struct PartitionNotFoundException : public std::exception{};


/*
 * Esta clase maneja el cifrado de exactamente un dispositivo, el montado de sus
 * particiones y su borrado.
 *
 * No puede usarse simultáneamente con distintos dispositivos. Sí se puede
 * "cerrar" y abrir otro dispositivo instanciando nuevamente esta clase.
 */
class PartitionManager
{
public:
    /*
     * Constructor:
     *
     * Crea una instancia de PartitionManager centrado en el dispositivo pasado
     * device. device es el path de un archivo de bloque Unix. (por ejemplo,
     * /dev/sda).
     *
     * Trata de desmontar todas las particiones montadas  que correspondan a
     * este dispositivo. Si en desmontar alguna, el constructor falla y lanza
     * una excepción.
     *
     * Chequea que se tenga acceso de escritura al archivo de bloque, crea el
     * wraparound si no existe. El wraparound es un dispositivo virtual que
     * repite los contenidos del dispositivo device una vez, generando un
     * dispositivo de tamaño doble cuyo path es /dev/mapper/wraparound.
     */
    PartitionManager(const std::string &device);
    ~PartitionManager();
    
    /*
     * wipeDevice:
     *
     * Rellena el archivo de bloque con información aleatoria. Si se llama 
	 * abortOperation() antes o durante la ejecución de este método,
     * la escritura se detiene.
     *
     * Siempre antes de llamar a este método, es necesario llamar a
     * resetProgress(). Si no se hace esto, no hay garantía de que el borrado
     * se realice.
     *
     * Devuelve verdadero si se hizo el borrado completo, falso si hubo un error
     * si hay una partición montada, si no se llamó a resetProgress(), o si fue
     * cancelado.
     */
    bool wipeDevice();

    /*
     * createPartition:
     *
     * Crea un filesystem en el dispositivo desencriptado con clave password,
     * escribiendo la cabezera del filesystem en el slot pasado.
     *
     * Retorna verdadero si crea exitosamente el filesystem y falso si ya hay
     * una partición montada, slot está fuera del rango [0,SLOTS_AMOUNT), si la
     * contraseña es un string vacío o si algún comando externo falla.
     */
    bool createPartition(const unsigned short slot,
                         const std::string &password);
    /*
     * mountPartition:
     *
     * Busca en cada uno de los slots una partición cifrada con la clave password
     * y la monta en /mnt/part en caso de encontrarla.
     * Este proceso se puede cancelar llamando concurrentemente al método
     * abortOperation().
     *
     * Retorna verdadero si encontró y montó la partición y falso si ya hay una
     * partición montada, la operación fue cancelada, o si falló alguna operación
     * interna.
     */
    bool mountPartition (const std::string &password);
    /*
     * unmountPartition:
     *
     * Desmonta la partición ya montada.
     *
     * Retorna verdadero en caso de desmontar exitosamente la partición y falso
     * si no hubiera una partición montada o si falla el comando de desmontado.
     */
    bool unmountPartition();

    /*
     * isPartitionMounted:
     *
     * Devuelve verdadero si la partición está montada y falso en caso contrario.
     */
    bool isPartitionMounted();

    /*
     * resetProgress:
     *
     * Prepara este objeto para la ejecución de wipeVolume().
     */
    void resetProgress();

    /*
     * getProgress:
     *
     * Devuelve el progreso [0,100] de wipeVolume().
     *
     * Esto método se puede llamar concurrentemente.
     */
    unsigned char getProgress() const;
    /*
     * abortOperation
     *
     * Inhibe la ejecución del siguiente llamado a wipeVolume() o
     * createPartition(). Si se llamara a este método durante la ejecución de
     * estos métodos, se detienen.
     *
     * Este método se puede llamar concurrentemente.
     */
    void abortOperation();

    /*
     * currentDeviceSize
     *
     * Devuelve el tamaño en sectores de 512 bytes del disco con el que fue
     * creado el objeto.
     *
     * Este método se puede llamar concurrentemente.
     */
    uint64_t currentDeviceSize();

    /*
     * ejectDevice:
     *
     * Si se llama este método en cualquier momento, el se cierra el
     * wraparound y se desmonta la partición al destruirse la instancia.
     */
    void ejectDevice();

    /*
     * findAllDevices:
     *
     * Devuelve una lista de strings con los paths a todos los dispositivos
     * SCSI.
     */
    static const std::list<std::string> findAllDevices();

    /*
     * currentDevice:
     *
     * Devuelve el path al dispositivo que tenga el wraparound abierto. Si no
     * hay ninguno, retorna una string vacía.
     */
    static const std::string currentDevice();

    static constexpr unsigned short SLOTS_AMOUNT = 8192;
    static const std::string WRAPAROUND;
    static const std::string MAPPER_DIR;
    static const std::string ENCRYPTED;
    static const std::string MOUNTPOINT;
private:
    std::string                 mCurrentDevice;
    // ambos en bloques
    uint64_t                    mDeviceSize;
    uint64_t                    mOffsetMultiple;
    std::atomic<unsigned short> mProgress;
    std::atomic<bool>           mOperationCanceled;
    std::mutex                  mGuard;
    bool                        mCloseAtDestroy;
    struct stat                 mFileStat;
    Crypto  mCrypto;

    void closeMapping(const std::string&);
    void openCryptMapping(const unsigned short slot,
                          const std::string &password);
    void createWraparound();
    bool isMountPoint(const std::string& dirPath);
    bool isWraparoundOurs();
    bool callMount();
    bool callUmount();
    void unmountAll();

    static const std::pair<unsigned,unsigned> currentDeviceID();

    static constexpr unsigned short BLOCK_SIZE_ = 512;
    const uint64_t MAX_WRITE_SIZE = 1 * (1 << (20-1)); // % 1 MiB
};


#endif
