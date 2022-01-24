/*
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SLAVEBASE_H
#define SLAVEBASE_H

#include "job_base.h" // for KIO::JobFlags
#include <kio/authinfo.h>
#include <kio/global.h>
#include <kio/udsentry.h>

#include <QByteArray>
#include <QHostInfo>

#include <memory>

class KConfigGroup;
class KRemoteEncoding;
class QUrl;

namespace KIO
{
class Connection;
class SlaveBasePrivate;

/**
 * @class KIO::SlaveBase slavebase.h <KIO/SlaveBase>
 *
 * There are two classes that specifies the protocol between application (job)
 * and kioslave. SlaveInterface is the class to use on the application end,
 * SlaveBase is the one to use on the slave end.
 *
 * Slave implementations should simply inherit SlaveBase
 *
 * A call to foo() results in a call to slotFoo() on the other end.
 *
 * Note that a kioslave doesn't have a Qt event loop. When idle, it's waiting for a command
 * on the socket that connects it to the application. So don't expect a kioslave to react
 * to D-Bus signals for instance. KIOSlaves are short-lived anyway, so any kind of watching
 * or listening for notifications should be done elsewhere, for instance in a kded module
 * (see kio_desktop's desktopnotifier.cpp for an example).
 *
 * If a kioslave needs a Qt event loop within the implementation of one method, e.g. to
 * wait for an asynchronous operation to finish, that is possible, using QEventLoop.
 */
class KIOCORE_EXPORT SlaveBase
{
public:
    SlaveBase(const QByteArray &protocol, const QByteArray &pool_socket, const QByteArray &app_socket);
    virtual ~SlaveBase();

    /**
     * @internal
     * Terminate the slave by calling the destructor and then ::exit()
     */
    Q_NORETURN void exit();

    /**
     * @internal
     */
    void dispatchLoop();

    ///////////
    // Message Signals to send to the job
    ///////////

    /**
     * Sends data in the slave to the job (i.e.\ in get).
     *
     * To signal end of data, simply send an empty
     * QByteArray().
     *
     * @param data the data read by the slave
     */
    void data(const QByteArray &data);

    /**
     * Asks for data from the job.
     * @see readData
     */
    void dataReq();

    /**
     * open succeeds
     * @see open()
     */
    void opened();

    /**
     * Call to signal an error.
     * This also finishes the job, so you must not call
     * finished() after calling this.
     *
     * If the error code is KIO::ERR_SLAVE_DEFINED then the
     * _text should contain the complete translated text of
     * of the error message.
     *
     * For all other error codes, _text should match the corresponding
     * error code. Usually, _text is a file or host name, or the error which
     * was passed from the server.<br>
     * For example, for KIO::ERR_DOES_NOT_EXIST, _text may only
     * be the file or folder which does not exist, nothing else. Otherwise,
     * this would break error strings generated by KIO::buildErrorString().<br>
     * If you have to add more details than what the standard error codes
     * provide, you'll need to use KIO::ERR_SLAVE_DEFINED.
     * For a complete list of what _text should contain for each error code,
     * look at the source of KIO::buildErrorString().
     *
     * You can add rich text markup to the message, the places where the
     * error message will be displayed are rich text aware.
     *
     * @see KIO::Error
     * @see KIO::buildErrorString
     * @param _errid the error code from KIO::Error
     * @param _text the rich text error message
     */
    void error(int _errid, const QString &_text);

    /**
     * Call in openConnection, if you reimplement it, when you're done.
     */
    void connected();

    /**
     * Call to signal successful completion of any command
     * besides openConnection and closeConnection. Do not
     * call this after calling error().
     */
    void finished();

    /**
     * Call to signal that data from the sub-URL is needed
     */
    void needSubUrlData();

    /**
     * Used to report the status of the slave.
     * @param host the slave is currently connected to. (Should be
     *        empty if not connected)
     * @param connected Whether an actual network connection exists.
     **/
    void slaveStatus(const QString &host, bool connected);

    /**
     * Call this from stat() to express details about an object, the
     * UDSEntry customarily contains the atoms describing file name, size,
     * MIME type, etc.
     * @param _entry The UDSEntry containing all of the object attributes.
     */
    void statEntry(const UDSEntry &_entry);

    /**
     * Call this in listDir, each time you have a bunch of entries
     * to report.
     * @param _entry The UDSEntry containing all of the object attributes.
     */
    void listEntries(const UDSEntryList &_entry);

    /**
     * Call this at the beginning of put(), to give the size of the existing
     * partial file, if there is one. The @p offset argument notifies the
     * other job (the one that gets the data) about the offset to use.
     * In this case, the boolean returns whether we can indeed resume or not
     * (we can't if the protocol doing the get() doesn't support setting an offset)
     */
    bool canResume(KIO::filesize_t offset);

    /**
     * Call this at the beginning of get(), if the "range-start" metadata was set
     * and returning byte ranges is implemented by this protocol.
     */
    void canResume();

    ///////////
    // Info Signals to send to the job
    ///////////

    /**
     * Call this in get and copy, to give the total size
     * of the file.
     */
    void totalSize(KIO::filesize_t _bytes);
    /**
     * Call this during get and copy, once in a while,
     * to give some info about the current state.
     * Don't emit it in listDir, listEntries speaks for itself.
     */
    void processedSize(KIO::filesize_t _bytes);

    void position(KIO::filesize_t _pos);

    void written(KIO::filesize_t _bytes);

    /**
     * @since 5.66
     */
    void truncated(KIO::filesize_t _length);

    /**
     * Only use this if you can't know in advance the size of the
     * copied data. For example, if you're doing variable bitrate
     * compression of the source.
     *
     * STUB ! Currently unimplemented. Here now for binary compatibility.
     *
     * Call this during get and copy, once in a while,
     * to give some info about the current state.
     * Don't emit it in listDir, listEntries speaks for itself.
     */
    void processedPercent(float percent); // KF6 TODO REMOVE

    /**
     * Call this in get and copy, to give the current transfer
     * speed, but only if it can't be calculated out of the size you
     * passed to processedSize (in most cases you don't want to call it)
     */
    void speed(unsigned long _bytes_per_second);

    /**
     * Call this to signal a redirection
     * The job will take care of going to that url.
     */
    void redirection(const QUrl &_url);

    /**
     * Tell that we will only get an error page here.
     * This means: the data you'll get isn't the data you requested,
     * but an error page (usually HTML) that describes an error.
     */
    void errorPage();

    /**
     * Call this in mimetype() and in get(), when you know the MIME type.
     * See mimetype() about other ways to implement it.
     */
    void mimeType(const QString &_type);

    /**
     * Call to signal a warning, to be displayed in a dialog box.
     */
    void warning(const QString &msg);

    /**
     * Call to signal a message, to be displayed if the application wants to,
     * for instance in a status bar. Usual examples are "connecting to host xyz", etc.
     */
    void infoMessage(const QString &msg);

    /**
     * Type of message box. Should be kept in sync with KMessageBox::DialogType.
     */
    enum MessageBoxType {
        QuestionYesNo = 1,
        WarningYesNo = 2,
        WarningContinueCancel = 3,
        WarningYesNoCancel = 4,
        Information = 5,
        SSLMessageBox = 6,
        // In KMessageBox::DialogType; Sorry = 7, Error = 8, QuestionYesNoCancel = 9
        WarningContinueCancelDetailed = 10,
    };

    /**
     * Button codes. Should be kept in sync with KMessageBox::ButtonCode
     */
    enum ButtonCode {
        Ok = 1,
        Cancel = 2,
        Yes = 3,
        No = 4,
        Continue = 5,
    };

    /**
     * Call this to show a message box from the slave
     * @param type type of message box: QuestionYesNo, WarningYesNo, WarningContinueCancel...
     * @param text Message string. May contain newlines.
     * @param caption Message box title.
     * @param buttonYes The text for the first button.
     *                  The default is i18n("&Yes").
     * @param buttonNo  The text for the second button.
     *                  The default is i18n("&No").
     * Note: for ContinueCancel, buttonYes is the continue button and buttonNo is unused.
     *       and for Information, none is used.
     * @return a button code, as defined in ButtonCode, or 0 on communication error.
     */
    int messageBox(MessageBoxType type,
                   const QString &text,
                   const QString &caption = QString(),
                   const QString &buttonYes = QString(),
                   const QString &buttonNo = QString());

    /**
     * Call this to show a message box from the slave
     * @param text Message string. May contain newlines.
     * @param type type of message box: QuestionYesNo, WarningYesNo, WarningContinueCancel...
     * @param caption Message box title.
     * @param buttonYes The text for the first button.
     *                  The default is i18n("&Yes").
     * @param buttonNo  The text for the second button.
     *                  The default is i18n("&No").
     * Note: for ContinueCancel, buttonYes is the continue button and buttonNo is unused.
     *       and for Information, none is used.
     * @param dontAskAgainName the name used to store result from 'Do not ask again' checkbox.
     * @return a button code, as defined in ButtonCode, or 0 on communication error.
     */
    int messageBox(const QString &text,
                   MessageBoxType type,
                   const QString &caption = QString(),
                   const QString &buttonYes = QString(),
                   const QString &buttonNo = QString(),
                   const QString &dontAskAgainName = QString());

    /**
     * Sets meta-data to be send to the application before the first
     * data() or finished() signal.
     */
    void setMetaData(const QString &key, const QString &value);

    /**
     * Queries for the existence of a certain config/meta-data entry
     * send by the application to the slave.
     */
    bool hasMetaData(const QString &key) const;

    /**
     * Queries for config/meta-data send by the application to the slave.
     */
    QString metaData(const QString &key) const;

    /**
     * @internal for ForwardingSlaveBase
     * Contains all metadata (but no config) sent by the application to the slave.
     */
    MetaData allMetaData() const;

    /**
     * Returns a map to query config/meta-data information from.
     *
     * The application provides the slave with all configuration information
     * relevant for the current protocol and host.
     *
     * Use configValue() as shortcut.
     * @since 5.64
     */
    QMap<QString, QVariant> mapConfig() const;

    /**
     * Returns a bool from the config/meta-data information.
     * @since 5.64
     */
    bool configValue(const QString &key, bool defaultValue) const;

    /**
     * Returns an int from the config/meta-data information.
     * @since 5.64
     */
    int configValue(const QString &key, int defaultValue) const;

    /**
     * Returns a QString from the config/meta-data information.
     * @since 5.64
     */
    QString configValue(const QString &key, const QString &defaultValue = QString()) const;

    /**
     * Returns a configuration object to query config/meta-data information
     * from.
     *
     * The application provides the slave with all configuration information
     * relevant for the current protocol and host.
     *
     * @note Since 5.64 prefer to use mapConfig() or one of the configValue(...) overloads.
     * @todo Find replacements for the other current usages of this method.
     */
    KConfigGroup *config();
    // KF6: perhaps rename mapConfig() to config() when removing this

    /**
     * Returns an object that can translate remote filenames into proper
     * Unicode forms. This encoding can be set by the user.
     */
    KRemoteEncoding *remoteEncoding();

    ///////////
    // Commands sent by the job, the slave has to
    // override what it wants to implement
    ///////////

    /**
     * Set the host
     *
     * Called directly by createSlave, this is why there is no equivalent in
     * SlaveInterface, unlike the other methods.
     *
     * This method is called whenever a change in host, port or user occurs.
     */
    virtual void setHost(const QString &host, quint16 port, const QString &user, const QString &pass);

    /**
     * Prepare slave for streaming operation
     */
    virtual void setSubUrl(const QUrl &url);

    /**
     * Opens the connection (forced)
     * When this function gets called the slave is operating in
     * connection-oriented mode.
     * When a connection gets lost while the slave operates in
     * connection oriented mode, the slave should report
     * ERR_CONNECTION_BROKEN instead of reconnecting. The user is
     * expected to disconnect the slave in the error handler.
     */
    virtual void openConnection();

    /**
     * Closes the connection (forced)
     * Called when the application disconnects the slave to close
     * any open network connections.
     *
     * When the slave was operating in connection-oriented mode,
     * it should reset itself to connectionless (default) mode.
     */
    virtual void closeConnection();

    /**
     * get, aka read.
     * @param url the full url for this request. Host, port and user of the URL
     *        can be assumed to be the same as in the last setHost() call.
     *
     * The slave should first "emit" the MIME type by calling mimeType(),
     * and then "emit" the data using the data() method.
     *
     * The reason why we need get() to emit the MIME type is:
     * when pasting a URL in krunner, or konqueror's location bar,
     * we have to find out what is the MIME type of that URL.
     * Rather than doing it with a call to mimetype(), then the app or part
     * would have to do a second request to the same server, this is done
     * like this: get() is called, and when it emits the MIME type, the job
     * is put on hold and the right app or part is launched. When that app
     * or part calls get(), the slave is magically reused, and the download
     * can now happen. All with a single call to get() in the slave.
     * This mechanism is also described in KIO::get().
     */
    virtual void get(const QUrl &url);

    /**
     * open.
     * @param url the full url for this request. Host, port and user of the URL
     *        can be assumed to be the same as in the last setHost() call.
     * @param mode see \ref QIODevice::OpenMode
     */
    virtual void open(const QUrl &url, QIODevice::OpenMode mode);

    /**
     * read.
     * @param size the requested amount of data to read
     * @see KIO::FileJob::read()
     */
    virtual void read(KIO::filesize_t size);
    /**
     * write.
     * @param data the data to write
     * @see KIO::FileJob::write()
     */
    virtual void write(const QByteArray &data);
    /**
     * seek.
     * @param offset the requested amount of data to read
     * @see KIO::FileJob::read()
     */
    virtual void seek(KIO::filesize_t offset);
    /**
     * close.
     * @see KIO::FileJob::close()
     */
    virtual void close();

    /**
     * put, i.e.\ write data into a file.
     *
     * @param url where to write the file
     * @param permissions may be -1. In this case no special permission mode is set.
     * @param flags We support Overwrite here. Hopefully, we're going to
     * support Resume in the future, too.
     * If the file indeed already exists, the slave should NOT apply the
     * permissions change to it.
     * The support for resuming using .part files is done by calling canResume().
     *
     * IMPORTANT: Use the "modified" metadata in order to set the modification time of the file.
     *
     * @see canResume()
     */
    virtual void put(const QUrl &url, int permissions, JobFlags flags);

    /**
     * Finds all details for one file or directory.
     * The information returned is the same as what listDir returns,
     * but only for one file or directory.
     * Call statEntry() after creating the appropriate UDSEntry for this
     * url.
     *
     * You can use the "details" metadata to optimize this method to only
     * do as much work as needed by the application.
     * By default details is 2 (all details wanted, including modification time, size, etc.),
     * details==1 is used when deleting: we don't need all the information if it takes
     * too much time, no need to follow symlinks etc.
     * details==0 is used for very simple probing: we'll only get the answer
     * "it's a file or a directory (or a symlink), or it doesn't exist".
     */
    virtual void stat(const QUrl &url);

    /**
     * Finds MIME type for one file or directory.
     *
     * This method should either emit 'mimeType' or it
     * should send a block of data big enough to be able
     * to determine the MIME type.
     *
     * If the slave doesn't reimplement it, a get will
     * be issued, i.e. the whole file will be downloaded before
     * determining the MIME type on it - this is obviously not a
     * good thing in most cases.
     */
    virtual void mimetype(const QUrl &url);

    /**
     * Lists the contents of @p url.
     * The slave should emit ERR_CANNOT_ENTER_DIRECTORY if it doesn't exist,
     * if we don't have enough permissions.
     * You should not list files if the path in @p url is empty, but redirect
     * to a non-empty path instead.
     */
    virtual void listDir(const QUrl &url);

    /**
     * Create a directory
     * @param url path to the directory to create
     * @param permissions the permissions to set after creating the directory
     * (-1 if no permissions to be set)
     * The slave emits ERR_CANNOT_MKDIR if failure.
     */
    virtual void mkdir(const QUrl &url, int permissions);

    /**
     * Rename @p oldname into @p newname.
     * If the slave returns an error ERR_UNSUPPORTED_ACTION, the job will
     * ask for copy + del instead.
     *
     * Important: the slave must implement the logic "if the destination already
     * exists, error ERR_DIR_ALREADY_EXIST or ERR_FILE_ALREADY_EXIST".
     * For performance reasons no stat is done in the destination before hand,
     * the slave must do it.
     *
     * By default, rename() is only called when renaming (moving) from
     * yourproto://host/path to yourproto://host/otherpath.
     *
     * If you set renameFromFile=true then rename() will also be called when
     * moving a file from file:///path to yourproto://host/otherpath.
     * Otherwise such a move would have to be done the slow way (copy+delete).
     * See KProtocolManager::canRenameFromFile() for more details.
     *
     * If you set renameToFile=true then rename() will also be called when
     * moving a file from yourproto: to file:.
     * See KProtocolManager::canRenameToFile() for more details.
     *
     * @param src where to move the file from
     * @param dest where to move the file to
     * @param flags We support Overwrite here
     */
    virtual void rename(const QUrl &src, const QUrl &dest, JobFlags flags);

    /**
     * Creates a symbolic link named @p dest, pointing to @p target, which
     * may be a relative or an absolute path.
     * @param target The string that will become the "target" of the link (can be relative)
     * @param dest The symlink to create.
     * @param flags We support Overwrite here
     */
    virtual void symlink(const QString &target, const QUrl &dest, JobFlags flags);

    /**
     * Change permissions on @p url
     * The slave emits ERR_DOES_NOT_EXIST or ERR_CANNOT_CHMOD
     */
    virtual void chmod(const QUrl &url, int permissions);

    /**
     * Change ownership of @p url
     * The slave emits ERR_DOES_NOT_EXIST or ERR_CANNOT_CHOWN
     */
    virtual void chown(const QUrl &url, const QString &owner, const QString &group);

    /**
     * Sets the modification time for @url
     * For instance this is what CopyJob uses to set mtime on dirs at the end of a copy.
     * It could also be used to set the mtime on any file, in theory.
     * The usual implementation on unix is to call utime(path, &myutimbuf).
     * The slave emits ERR_DOES_NOT_EXIST or ERR_CANNOT_SETTIME
     */
    virtual void setModificationTime(const QUrl &url, const QDateTime &mtime);

    /**
     * Copy @p src into @p dest.
     *
     * By default, copy() is only called when copying a file from
     * yourproto://host/path to yourproto://host/otherpath.
     *
     * If you set copyFromFile=true then copy() will also be called when
     * moving a file from file:///path to yourproto://host/otherpath.
     * Otherwise such a copy would have to be done the slow way (get+put).
     * See also KProtocolManager::canCopyFromFile().
     *
     * If you set copyToFile=true then copy() will also be called when
     * moving a file from yourproto: to file:.
     * See also KProtocolManager::canCopyToFile().
     *
     * If the slave returns an error ERR_UNSUPPORTED_ACTION, the job will
     * ask for get + put instead.
     *
     * If the slave returns an error ERR_FILE_ALREADY_EXIST, the job will
     * ask for a different destination filename.
     *
     * @param src where to copy the file from (decoded)
     * @param dest where to copy the file to (decoded)
     * @param permissions may be -1. In this case no special permission mode is set,
     *        and the owner and group permissions are not preserved.
     * @param flags We support Overwrite here
     *
     * Don't forget to set the modification time of @p dest to be the modification time of @p src.
     */
    virtual void copy(const QUrl &src, const QUrl &dest, int permissions, JobFlags flags);

    /**
     * Delete a file or directory.
     * @param url file/directory to delete
     * @param isfile if true, a file should be deleted.
     *               if false, a directory should be deleted.
     *
     * By default, del() on a directory should FAIL if the directory is not empty.
     * However, if metadata("recurse") == "true", then the slave can do a recursive deletion.
     * This behavior is only invoked if the slave specifies deleteRecursive=true in its protocol file.
     */
    virtual void del(const QUrl &url, bool isfile);

    /**
     * Change the destination of a symlink
     * @param url the url of the symlink to modify
     * @param target the new destination (target) of the symlink
     */
    virtual void setLinkDest(const QUrl &url, const QString &target);

    /**
     * Used for any command that is specific to this slave (protocol)
     * Examples are : HTTP POST, mount and unmount (kio_file)
     *
     * @param data packed data; the meaning is completely dependent on the
     *        slave, but usually starts with an int for the command number.
     * Document your slave's commands, at least in its header file.
     */
    virtual void special(const QByteArray &data);

    /**
     * Used for multiple get. Currently only used for HTTP pipelining
     * support.
     *
     * @param data packed data; Contains number of URLs to fetch, and for
     * each URL the URL itself and its associated MetaData.
     */
    virtual void multiGet(const QByteArray &data);

    /**
     * Called to get the status of the slave. Slave should respond
     * by calling slaveStatus(...)
     */
    virtual void slave_status();

    /**
     * Called by the scheduler to tell the slave that the configuration
     * changed (i.e.\ proxy settings).
     */
    virtual void reparseConfiguration();

    /**
     * @return timeout value for connecting to remote host.
     */
    int connectTimeout();

    /**
     * @return timeout value for connecting to proxy in secs.
     */
    int proxyConnectTimeout();

    /**
     * @return timeout value for read from first data from
     * remote host in seconds.
     */
    int responseTimeout();

    /**
     * @return timeout value for read from subsequent data from
     * remote host in secs.
     */
    int readTimeout();

    /**
     * This function sets a timeout of @p timeout seconds and calls
     * special(data) when the timeout occurs as if it was called by the
     * application.
     *
     * A timeout can only occur when the slave is waiting for a command
     * from the application.
     *
     * Specifying a negative timeout cancels a pending timeout.
     *
     * Only one timeout at a time is supported, setting a timeout
     * cancels any pending timeout.
     */
    void setTimeoutSpecialCommand(int timeout, const QByteArray &data = QByteArray());

    /////////////////
    // Dispatching (internal)
    ////////////////

    /**
     * @internal
     */
    virtual void dispatch(int command, const QByteArray &data);

    /**
     * @internal
     */
    virtual void dispatchOpenCommand(int command, const QByteArray &data);

    /**
     * Read data sent by the job, after a dataReq
     *
     * @param buffer buffer where data is stored
     * @return 0 on end of data,
     *         > 0 bytes read
     *         < 0 error
     **/
    int readData(QByteArray &buffer);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     * It collects entries and emits them via listEntries
     * when enough of them are there or a certain time
     * frame exceeded (to make sure the app gets some
     * items in time but not too many items one by one
     * as this will cause a drastic performance penalty).
     * @param _entry The UDSEntry containing all of the object attributes.
     * @param ready set to true after emitting all items. @p _entry is not
     *        used in this case
     * @deprecated since 5.0. the listEntry(entry, true) indicated
     * that the entry listing was completed. However, each slave should
     * already call finished() to also tell us that we're done listing.
     * You should make sure that finished() is called when the entry
     * listing is completed and simply remove the call to listEntry(entry, true).
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "See API docs")
    void listEntry(const UDSEntry &_entry, bool ready);
#endif

    /**
     * It collects entries and emits them via listEntries
     * when enough of them are there or a certain time
     * frame exceeded (to make sure the app gets some
     * items in time but not too many items one by one
     * as this will cause a drastic performance penalty).
     * @param entry The UDSEntry containing all of the object attributes.
     * @since 5.0
     */
    void listEntry(const UDSEntry &entry);

    /**
     * internal function to connect a slave to/ disconnect from
     * either the slave pool or the application
     */
    void connectSlave(const QString &path);
    void disconnectSlave();

    /**
     * Prompt the user for Authorization info (login & password).
     *
     * Use this function to request authorization information from
     * the end user. You can also pass an error message which explains
     * why a previous authorization attempt failed. Here is a very
     * simple example:
     *
     * \code
     * KIO::AuthInfo authInfo;
     * int errorCode = openPasswordDialogV2(authInfo);
     * if (!errorCode) {
     *    qDebug() << QLatin1String("User: ") << authInfo.username;
     *    qDebug() << QLatin1String("Password: not displayed here!");
     * } else {
     *    error(errorCode, QString());
     * }
     * \endcode
     *
     * You can also preset some values like the username, caption or
     * comment as follows:
     *
     * \code
     * KIO::AuthInfo authInfo;
     * authInfo.caption = i18n("Acme Password Dialog");
     * authInfo.username = "Wile E. Coyote";
     * QString errorMsg = i18n("You entered an incorrect password.");
     * int errorCode = openPasswordDialogV2(authInfo, errorMsg);
     * [...]
     * \endcode
     *
     * \note You should consider using checkCachedAuthentication() to
     * see if the password is available in kpasswdserver before calling
     * this function.
     *
     * \note A call to this function can fail and return @p false,
     * if the password server could not be started for whatever reason.
     *
     * \note This function does not store the password information
     * automatically (and has not since kdelibs 4.7). If you want to
     * store the password information in a persistent storage like
     * KWallet, then you MUST call @ref cacheAuthentication.
     *
     * @see checkCachedAuthentication
     * @param info  See AuthInfo.
     * @param errorMsg Error message to show
     * @return a KIO error code: NoError (0), KIO::USER_CANCELED, or other error codes.
     */
    int openPasswordDialogV2(KIO::AuthInfo &info, const QString &errorMsg = QString());

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 24)
    /**
     * @deprecated since KF 5.24, use openPasswordDialogV2.
     * The return value works differently:
     *  instead of
     *        if (!openPasswordDialog()) { error(USER_CANCELED); }
     *  store and pass the return value to error(), when NOT zero,
     *  as shown documentation for openPasswordDialogV2().
     */
    KIOCORE_DEPRECATED_VERSION(5, 24, "Use SlaveBase::openPasswordDialogV2(...)")
    bool openPasswordDialog(KIO::AuthInfo &info, const QString &errorMsg = QString());
#endif

    /**
     * Checks for cached authentication based on parameters
     * given by @p info.
     *
     * Use this function to check if any cached password exists
     * for the URL given by @p info.  If @p AuthInfo::realmValue
     * and/or @p AuthInfo::verifyPath flag is specified, then
     * they will also be factored in determining the presence
     * of a cached password.  Note that @p Auth::url is a required
     * parameter when attempting to check for cached authorization
     * info. Here is a simple example:
     *
     * \code
     * AuthInfo info;
     * info.url = QUrl("http://www.foobar.org/foo/bar");
     * info.username = "somename";
     * info.verifyPath = true;
     * if ( !checkCachedAuthentication( info ) )
     * {
     *    int errorCode = openPasswordDialogV2(info);
     *     ....
     * }
     * \endcode
     *
     * @param       info See AuthInfo.
     * @return      @p true if cached Authorization is found, false otherwise.
     */
    bool checkCachedAuthentication(AuthInfo &info);

    /**
     * Caches @p info in a persistent storage like KWallet.
     *
     * Note that calling openPasswordDialogV2 does not store passwords
     * automatically for you (and has not since kdelibs 4.7).
     *
     * Here is a simple example of how to use cacheAuthentication:
     *
     * \code
     * AuthInfo info;
     * info.url = QUrl("http://www.foobar.org/foo/bar");
     * info.username = "somename";
     * info.verifyPath = true;
     * if ( !checkCachedAuthentication( info ) ) {
     *    int errorCode = openPasswordDialogV2(info);
     *    if (!errorCode) {
     *        if (info.keepPassword)  {  // user asked password be save/remembered
     *             cacheAuthentication(info);
     *        }
     *    }
     * }
     * \endcode
     *
     * @param info See AuthInfo.
     * @return @p true if @p info was successfully cached.
     */
    bool cacheAuthentication(const AuthInfo &info);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     * Used by the slave to check if it can connect
     * to a given host. This should be called where the slave is ready
     * to do a ::connect() on a socket. For each call to
     * requestNetwork must exist a matching call to
     * dropNetwork, or the system will stay online until
     * KNetMgr gets closed (or the SlaveBase gets destructed)!
     *
     * If KNetMgr is not running, then this is a no-op and returns true
     *
     * @param host tells the netmgr the host the slave wants to connect
     *             to. As this could also be a proxy, we can't just take
     *             the host currently connected to (but that's the default
     *             value)
     *
     * @return true in theory, the host is reachable
     *         false the system is offline and the host is in a remote network.
     *
     * @deprecated Since 5.0, for a very very long time, not implemented anymore
     * Probably dates back to model dialup times.
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "Not implemented & used")
    bool requestNetwork(const QString &host = QString());
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     *
     * Used by the slave to withdraw a connection requested by
     * requestNetwork. This function cancels the last call to
     * requestNetwork. If a client uses more than one internet
     * connection, it must use dropNetwork(host) to
     * stop each request.
     *
     * If KNetMgr is not running, then this is a no-op.
     *
     * @param host the host passed to requestNetwork
     *
     * A slave should call this function every time it disconnect from a host.
     *
     * @deprecated Since 5.0, for a very very long time, not implemented anymore
     * Probably dates back to model dialup times.
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "Not implemented & used")
    void dropNetwork(const QString &host = QString());
#endif

    /**
     * Wait for an answer to our request, until we get @p expected1 or @p expected2
     * @return the result from readData, as well as the cmd in *pCmd if set, and the data in @p data
     */
    int waitForAnswer(int expected1, int expected2, QByteArray &data, int *pCmd = nullptr);

    /**
     * Internal function to transmit meta data to the application.
     * m_outgoingMetaData will be cleared; this means that if the slave is for
     * example put on hold and picked up by a different KIO::Job later the new
     * job will not see the metadata sent before.
     * See kio/DESIGN.krun for an overview of the state
     * progression of a job/slave.
     * @warning calling this method may seriously interfere with the operation
     * of KIO which relies on the presence of some metadata at some points in time.
     * You should not use it if you are not familiar with KIO and not before
     * the slave is connected to the last job before returning to idle state.
     */
    void sendMetaData();

    /**
     * Internal function to transmit meta data to the application.
     * Like sendMetaData() but m_outgoingMetaData will not be cleared.
     * This method is mainly useful in code that runs before the slave is connected
     * to its final job.
     */
    void sendAndKeepMetaData();

    /** If your ioslave was killed by a signal, wasKilled() returns true.
     Check it regularly in lengthy functions (e.g. in get();) and return
     as fast as possible from this function if wasKilled() returns true.
     This will ensure that your slave destructor will be called correctly.
     */
    bool wasKilled() const;

    /** Internally used.
     * @internal
     */
    void setKillFlag();

    /** Internally used
     * @internal
     */
    void lookupHost(const QString &host);

    /** Internally used
     * @internal
     */
    int waitForHostInfo(QHostInfo &info);

    /**
     * Checks with job if privilege operation is allowed.
     * @return privilege operation status.
     * @see PrivilegeOperationStatus
     * @since 5.66
     */
    PrivilegeOperationStatus requestPrivilegeOperation(const QString &operationDetails);

    /**
     * Adds @p action to the list of PolicyKit actions which the
     * slave is authorized to perform.
     *
     * @param action the PolicyKit action
     * @since 5.45
     */
    void addTemporaryAuthorization(const QString &action);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 66)
    /**
     * @deprecated since 5.66, use requestPrivilegeOperation(QString)
     */
    KIOCORE_DEPRECATED_VERSION(5, 66, "Pass QString action to requestPrivilegeOperation")
    PrivilegeOperationStatus requestPrivilegeOperation();
#endif

protected:
    /**
     * Name of the protocol supported by this slave
     */
    QByteArray mProtocol;
    // Often used by TcpSlaveBase and unlikely to change
    MetaData mOutgoingMetaData;
    MetaData mIncomingMetaData;

    enum VirtualFunctionId {
        AppConnectionMade = 0,
        GetFileSystemFreeSpace = 1, // KF6 TODO: Turn into a virtual method
        Truncate = 2, // KF6 TODO: Turn into a virtual method
    };
    virtual void virtual_hook(int id, void *data);

private:
    // Convenience function converting mProtocol to QString as unsupportedActionErrorString(), which
    // is used in many places in the code, takes a QString parameter
    inline const QString protocolName() const
    {
        return QString::fromLatin1(mProtocol);
    }

    // This helps catching missing tr()/i18n() calls in error().
    void error(int _errid, const QByteArray &_text);
    void send(int cmd, const QByteArray &arr = QByteArray());
    std::unique_ptr<SlaveBasePrivate> const d;
    friend class SlaveBasePrivate;
};

/**
 * Returns an appropriate error message if the given command @p cmd
 * is an unsupported action (ERR_UNSUPPORTED_ACTION).
 * @param protocol name of the protocol
 * @param cmd given command
 * @see enum Command
 */
KIOCORE_EXPORT QString unsupportedActionErrorString(const QString &protocol, int cmd);
}

#endif
