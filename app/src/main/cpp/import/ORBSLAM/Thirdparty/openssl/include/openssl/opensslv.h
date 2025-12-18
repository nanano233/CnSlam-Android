#ifndef HEADER_OPENSSLV_H
#define HEADER_OPENSSLV_H

#ifdef  __cplusplus
extern "C" {
#endif

/* * 兼容旧代码的版本号宏
 * 格式: 0xMNN00PP0L (M=Major, NN=Minor, PP=Patch)
 * 3.6.0 -> 0x30600000L
 */
#define OPENSSL_VERSION_NUMBER 0x30600000L
#define OPENSSL_VERSION_TEXT   "OpenSSL 3.6.0"

/* OpenSSL 3.x 系列新增的版本宏 */
#define OPENSSL_VERSION_MAJOR  3
#define OPENSSL_VERSION_MINOR  6
#define OPENSSL_VERSION_PATCH  0
#define OPENSSL_VERSION_PRE_RELEASE ""
#define OPENSSL_VERSION_BUILD_METADATA ""

/* 辅助宏，确保编译通过 */
#define SHLIB_VERSION_HISTORY ""
#define SHLIB_VERSION_NUMBER "3"

#ifdef  __cplusplus
}
#endif

#endif /* HEADER_OPENSSLV_H */