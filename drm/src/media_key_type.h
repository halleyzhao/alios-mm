/**
 * Copyright (C) 2017 Alibaba Group Holding Limited. All Rights Reserved.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __media_key_type_h
#define __media_key_type_h

#include <vector>
#include <string>
#include <list>
#include <map>

#include <stdint.h>
#include <stdio.h>

// TODO rename to media_key_def.h
namespace YUNOS_DRM {

enum MediaKeySessionType {
    /**
     * A session for which the license and record of or data related to the
     * session MUST NOT be persisted. The application need not worry about
     * managing such storage.
     */
    SessionTypeTemporary,
    /**
     * TBD
     */
    SessionTypePersistentUsageRecord,
    /**
     * TBD
     */
    SessionTypePersistentLicense
};

enum MediaKeyStatus {
    /**
     * The key is currently usable to decrypt media data
     */
    MediaKeyUsable,
    /**
     * The key is no longer usable to decrypted media data
     * because its expiration time has passed
     */
    MediaKeyExpired,
    /**
     * The key itself is not available to the DRM module but
     * information such as record of key usage could be available
     */
    MediaKeyReleased,
    /**
     * There are output restrictions associated with the key that cannot
     * currently be met. Media data decrypted with this key may be blocked
     * from presentation. The application should avoid using streams that
     * will trigger the output restrictions associated with the key
     */
    MediaKeyOutputRestricted,
    /**
     * There are output restrictions associated with the key that cannot
     * currently be met. Media data decrypted with this key may be presented
     * with lower quality
     */
    MediaKeyOutputDownscaled,
    /**
     * The status of the key is not yet known and is being determined. The
     * status will be updated with the actual status when it has been determined
     */
    MediaKeyStatusPending,
    /**
     * The key is not currently usable to decrypt media data because of an error
     * in the DRM module unrelated to the other values
     */
    MediaKeyInternalError,
    /**
     * Key ID is unknown
     */
    MediaKeyUnknown
};

const char* keyStatusToString(MediaKeyStatus status);

enum MediaKeyMessageEvent {
    /**
     * The message contains a request for new license
     */
    EventLicenseRequest,
    /**
     * The message contains a request to renew a existing license
     */
    EventLicenseRenewal,
    /**
     * The message contains a record of license destruction
     */
    EventLicenseRelease,
    /**
     * The message contains a request for provision
     * Message format "defaultUrl:%s request:%s"
     */
    EventIndividualizationRequest
};
const char* keyEventToString(MediaKeyMessageEvent keyEvent);

enum Event {
    /**
     * There has been a change in the keys in the session or their status
     * which could be happen as a result of load() or update() call. Need to
     * check MediaKeyStatus of specified session for details
     * @param param1: undefined
     */
    EventKeyStatusesChange = EventIndividualizationRequest + 1,
    /**
     * There has been a error
     * @param param1: error code of enum ErrorType
     */
    EventError,
    /**
     * There has been a drm session expiration update,
     * client should call expiration() to get session expiration time
     * @param param1: undefined
     */
};
const char* eventToString(Event event);

enum ErrorType {
    /**
     * The existing MediaKeys object cannot be removed.
     * The key system is not supported.
     * The key system is not supported in an insecure context.
     * The initialization data type is not supported by the key system.
     * The session type is not supported by the key system.
     * The initialization data is not supported by the key system.
     * The operation is not supported by the key system.
     */
    NotSupportedError = EventError + 1,
    /**
     * The existing MediaKeys object cannot be removed at this time.
     * The session has already been used.
     * The session is not yet initialized.
     * The session is closed.
     */
    InvalidStateError,
    /**
     * The parameter is empty.
     * Invalid initialization data.
     * Invalid response format.
     * A persistent license was provided for a temporary session. 
     */
    TypeError,
    /**
     * Indicates a value that is not in the set or range of allowable
     * values. For example, the operation is not supported on sessions
     * of this type
     */
    RangeError,
    /**
     * A non-closed session already exists for this sessionId
     */
    QuotaExceedError
};
const char* errorToString(ErrorType err);

enum Mode {
    ModeUnencrypted,
    /**
     * AES CTR
     */
    ModeAESCTR,
    /**
     * AES CBC
     */
    ModeAESCBC,
    /**
     * AES with fixed key and iv
     */
    ModeAESFIXED,
};

struct SubSample {
    uint32_t mNumBytesOfClearData;
    uint32_t mNumBytesOfEncryptedData;
};

typedef std::vector<uint8_t> KeyID;
typedef std::vector<uint8_t> SessionID;
typedef std::vector<uint8_t> Message;

typedef int32_t status_t;

enum {
    // general status code
    OK                = 0,    // Everything's swell.
    NO_ERROR          = 0,    // No errors.

    UNKNOWN_ERROR       = (-2147483647-1), // INT32_MIN value
    NO_MEMORY           = -1,
    INVALID_OPERATION   = -2,
    BAD_VALUE           = -3,
    NAME_NOT_FOUND      = -4,
    PERMISSION_DENIED   = -5,
    NO_INIT             = -6,
    ALREADY_EXISTS      = -7,
    BAD_INDEX           = -8,
    BAD_TYPE            = (UNKNOWN_ERROR + 1),
    NOT_ENOUGH_DATA     = (UNKNOWN_ERROR + 3),
    WOULD_BLOCK         = (UNKNOWN_ERROR + 4),
    TIMED_OUT           = (UNKNOWN_ERROR + 5),

    // DRM specific error
    DRM_ERROR_START = -2000,
    DRM_ERROR_UNKNOWN                        = DRM_ERROR_START,
    DRM_ERROR_NO_LICENSE                     = DRM_ERROR_START - 1,
    DRM_ERROR_LICENSE_EXPIRED                = DRM_ERROR_START - 2,
    DRM_ERROR_SESSION_NOT_OPENED             = DRM_ERROR_START - 3,
    DRM_ERROR_DECRYPT_UNIT_NOT_INITIALIZED   = DRM_ERROR_START - 4,
    DRM_ERROR_DECRYPT                        = DRM_ERROR_START - 5,
    DRM_ERROR_CANNOT_HANDLE                  = DRM_ERROR_START - 6,
    DRM_ERROR_TAMPER_DETECTED                = DRM_ERROR_START - 7,
    DRM_ERROR_NOT_PROVISIONED                = DRM_ERROR_START - 8,
    DRM_ERROR_DEVICE_REVOKED                 = DRM_ERROR_START - 9,
    DRM_ERROR_RESOURCE_BUSY                  = DRM_ERROR_START - 10,
    DRM_ERROR_INSUFFICIENT_OUTPUT_PROTECTION = DRM_ERROR_START - 11,
    DRM_ERROR_LAST_USED_ERRORCODE            = DRM_ERROR_START - 12,

    DRM_ERROR_VENDOR_MAX                     = DRM_ERROR_START - 200,
    DRM_ERROR_VENDOR_MIN                     = DRM_ERROR_START - 399,
};

#define ERROR_DRM_UNKNOWN                           DRM_ERROR_UNKNOWN
#define ERROR_DRM_DECRYPT_UNIT_NOT_INITIALIZED      DRM_ERROR_DECRYPT_UNIT_NOT_INITIALIZED
#define ERROR_DRM_LICENSE_EXPIRED                   DRM_ERROR_LICENSE_EXPIRED
#define ERROR_DRM_SESSION_NOT_OPENED                DRM_ERROR_SESSION_NOT_OPENED
#define ERROR_DRM_CANNOT_HANDLE                     DRM_ERROR_CANNOT_HANDLE
#define ERROR_DRM_NOT_PROVISIONED                   DRM_ERROR_NOT_PROVISIONED
#define ERROR_DRM_DECRYPT                           DRM_ERROR_DECRYPT
#define ERROR_DRM_NO_LICENSE                        DRM_ERROR_NO_LICENSE
#define ERROR_DRM_RESOURCE_BUSY                     DRM_ERROR_RESOURCE_BUSY
#define ERROR_DRM_TAMPER_DETECTED                   DRM_ERROR_TAMPER_DETECTED
#define ERROR_DRM_DEVICE_REVOKED                    DRM_ERROR_DEVICE_REVOKED
#define ERROR_DRM_INSUFFICIENT_OUTPUT_PROTECTION    DRM_ERROR_INSUFFICIENT_OUTPUT_PROTECTION
#define ERROR_DRM_LAST_USED_ERRORCODE               DRM_ERROR_LAST_USED_ERRORCODE
#define ERROR_DRM_VENDOR_MAX                        DRM_ERROR_VENDOR_MAX
#define ERROR_DRM_VENDOR_MIN                        DRM_ERROR_VENDOR_MIN

#define DRM_DISALLOW_COPY(type) \
private: \
    type(const type &); \
    type & operator=(const type &);

} // end of namespace YUNOS_DRM
#endif //__media_key_type_h
