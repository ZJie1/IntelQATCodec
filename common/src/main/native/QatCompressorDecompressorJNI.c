/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "qatcodec.h"

#if defined LUCENE_QAT_LIBRARY

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#ifdef WINDOWS
#include "winutils.h"
#endif

#include "com_intel_qat_jni_QatCompressorDecompressorJNI.h"

#define JINT_MAX 0x7fffffff
#define qaePinnedMemAlloc(x, y)  qaeMemAllocNUMA((x), (y), 8)
#define qaePinnedMemFree(x)      qaeMemFreeNUMA((void **)&(x))

static jfieldID QatCompressorDecompressorJNI_clazz;
static jfieldID QatCompressorDecompressorJNI_uncompressedDirectBuf;
static jfieldID QatCompressorDecompressorJNI_uncompressedDirectBufLen;
static jfieldID QatCompressorDecompressorJNI_compressedDirectBuf;
static jfieldID QatCompressorDecompressorJNI_compressedDirectBufLen;
static jfieldID QatCompressorDecompressorJNI_directBufferSize;

__thread QzSession_T  g_qzSession = {
    .internal = NULL,
};

#ifdef UNIX
unsigned char* (*dlsym_qzMalloc)(int, int, int);
static int (*dlsym_qzCompress)(QzSession_T *sess, const unsigned char* src,
    unsigned int* src_len, unsigned char* dest, unsigned int* dest_len,
    unsigned int last);

static int (*dlsym_qzDecompress)(QzSession_T *sess, const unsigned char* src,
    unsigned int* compressed_buf_len, unsigned char* dest,
    unsigned int* uncompressed_buffer_len);
    unsigned char* (*dlsym_qzMalloc)(int sz, int numa, int force_pinned);
    int (*dlsym_qzGetDefaults)(QzSessionParams_T *defaults);
    int (*dlsym_qzSetDefaults)(QzSessionParams_T *defaults);
#endif

JNIEXPORT void JNICALL Java_com_intel_qat_jni_QatCompressorDecompressorJNI_initIDs
(JNIEnv *env, jclass clazz, jint level){
  QzSessionParams_T params;
#ifdef UNIX
  // Load libqatzip.so
  void *libqatzip = dlopen("libqatzip.so", RTLD_LAZY | RTLD_GLOBAL);
  if (!libqatzip) {
    char msg[1000];
    snprintf(msg, 1000, "%s (%s)!", "Cannot load " LUCENE_QAT_LIBRARY, dlerror());
    THROW(env, "java/lang/UnsatisfiedLinkError1", msg);
    return;
  }
#endif

  // Locate the requisite symbols from libqatzip.so
#ifdef UNIX
  dlerror();                                 // Clear any existing error
  LOAD_DYNAMIC_SYMBOL(dlsym_qzCompress, env, libqatzip, "qzCompress");
  LOAD_DYNAMIC_SYMBOL(dlsym_qzMalloc, env, libqatzip, "qzMalloc");
  LOAD_DYNAMIC_SYMBOL(dlsym_qzGetDefaults, env, libqatzip, "qzGetDefaults");
  LOAD_DYNAMIC_SYMBOL(dlsym_qzSetDefaults, env, libqatzip, "qzSetDefaults");
#endif

  QatCompressorDecompressorJNI_clazz = (*env)->GetStaticFieldID(env, clazz, "clazz",
                                                 "Ljava/lang/Class;");
  QatCompressorDecompressorJNI_uncompressedDirectBuf = (*env)->GetFieldID(env, clazz,
                                                           "uncompressedDirectBuf",
                                                           "Ljava/nio/Buffer;");
  QatCompressorDecompressorJNI_uncompressedDirectBufLen = (*env)->GetFieldID(env, clazz,
                                                              "uncompressedDirectBufLen", "I");
  QatCompressorDecompressorJNI_compressedDirectBuf = (*env)->GetFieldID(env, clazz,
                                                         "compressedDirectBuf",
                                                         "Ljava/nio/Buffer;");
  QatCompressorDecompressorJNI_directBufferSize = (*env)->GetFieldID(env, clazz,
                                                       "directBufferSize", "I");
  dlsym_qzGetDefaults(&params);
  params.comp_lvl = level;
  fprintf(stderr, "compression level is %d, tid is %d\n", level, syscall(__NR_gettid));
  dlsym_qzSetDefaults(&params);
}

JNIEXPORT void JNICALL Java_com_intel_qat_jni_QatCompressorDecompressorJNI_initIDsNoLevel
(JNIEnv *env, jclass clazz){
  // Load libqatzip.so
#ifdef UNIX
  void *libqatzip = dlopen("libqatzip.so", RTLD_LAZY | RTLD_GLOBAL);
  if (!libqatzip) {
    char msg[1000];
    snprintf(msg, sizeof(msg), "%s (%s)!", "Cannot load " LUCENE_QAT_LIBRARY, dlerror());
    THROW(env, "java/lang/UnsatisfiedLinkError", msg);
    return;
  }
#endif

#ifdef WINDOWS
  HMODULE libqatzip = LoadLibrary(LUCENE_QAT_LIBRARY);
  if (!libqatzip) {
    THROW(env, "java/lang/UnsatisfiedLinkError", "Cannot load qatzip.dll");
    return;
  }
#endif

  // Locate the requisite symbols from libqatzip.so
#ifdef UNIX
  dlerror();                                 // Clear any existing error
  LOAD_DYNAMIC_SYMBOL(dlsym_qzDecompress, env, libqatzip, "qzDecompress");
  LOAD_DYNAMIC_SYMBOL(dlsym_qzMalloc, env, libqatzip, "qzMalloc");
  LOAD_DYNAMIC_SYMBOL(dlsym_qzGetDefaults, env, libqatzip, "qzGetDefaults");
  LOAD_DYNAMIC_SYMBOL(dlsym_qzSetDefaults, env, libqatzip, "qzSetDefaults");
#endif

#ifdef WINDOWS
  LOAD_DYNAMIC_SYMBOL(__dlsym_qatzip_uncompress, dlsym_qzDecompress, env, libqatzip, "qzDecompress");
#endif

  fprintf(stderr, "decompression tid is %d\n",syscall(__NR_gettid));
  fflush(stderr);
  QatCompressorDecompressorJNI_clazz = (*env)->GetStaticFieldID(env, clazz, "clazz",
                                                   "Ljava/lang/Class;");
  QatCompressorDecompressorJNI_compressedDirectBuf = (*env)->GetFieldID(env,clazz,
                                                           "compressedDirectBuf",
                                                           "Ljava/nio/Buffer;");
  QatCompressorDecompressorJNI_compressedDirectBufLen = (*env)->GetFieldID(env,clazz,
                                                              "compressedDirectBufLen", "I");
  QatCompressorDecompressorJNI_uncompressedDirectBuf = (*env)->GetFieldID(env,clazz,
                                                             "uncompressedDirectBuf",
                                                             "Ljava/nio/Buffer;");
  QatCompressorDecompressorJNI_directBufferSize = (*env)->GetFieldID(env, clazz,
                                                         "directBufferSize", "I");
}

JNIEXPORT jint JNICALL Java_com_intel_qat_jni_QatCompressorDecompressorJNI_compressBytesDirect
(JNIEnv *env, jobject thisj){
  const unsigned char* uncompressed_bytes;
  unsigned char* compressed_bytes;
  int ret;
  // Get members of QatCompressorDecompressorJNI
  jobject clazz = (*env)->GetStaticObjectField(env, thisj, QatCompressorDecompressorJNI_clazz);
  jobject uncompressed_direct_buf = (*env)->GetObjectField(env, thisj, QatCompressorDecompressorJNI_uncompressedDirectBuf);
  jint uncompressed_direct_buf_len = (*env)->GetIntField(env, thisj, QatCompressorDecompressorJNI_uncompressedDirectBufLen);
  jobject compressed_direct_buf = (*env)->GetObjectField(env, thisj, QatCompressorDecompressorJNI_compressedDirectBuf);
  jint compressed_direct_buf_len = (*env)->GetIntField(env, thisj, QatCompressorDecompressorJNI_directBufferSize);
  unsigned int buf_len;
  unsigned int src_len;

  // Get the input direct buffer
  LOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");
  uncompressed_bytes = (const unsigned char*)(*env)->GetDirectBufferAddress(env, uncompressed_direct_buf);
  UNLOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");

  if (uncompressed_bytes == 0) {
    return (jint)0;
  }

  // Get the output direct buffer
  LOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");
  compressed_bytes = (unsigned char *)(*env)->GetDirectBufferAddress(env, compressed_direct_buf);
  UNLOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");

  if (compressed_bytes == 0) {
    return (jint)0;
  }

  /* size_t should always be 4 bytes or larger. */
  buf_len = compressed_direct_buf_len;
  src_len = uncompressed_direct_buf_len;
  ret = dlsym_qzCompress(&g_qzSession, uncompressed_bytes, &src_len,
        compressed_bytes, &buf_len, 1);

  if (ret != QZ_OK){
    THROW(env, "java/lang/InternalError", "Could not compress data, return " + ret);
    return 0;
  }
  if (buf_len > JINT_MAX) {
    THROW(env, "java/lang/InternalError", "Invalid return buffer length.");
    return 0;
  }

  (*env)->SetIntField(env, thisj, QatCompressorDecompressorJNI_uncompressedDirectBufLen, 0);
  return (jint)buf_len;
}

JNIEXPORT jint JNICALL Java_com_intel_qat_jni_QatCompressorDecompressorJNI_decompressBytesDirect
(JNIEnv *env, jobject thisj){

  const unsigned char* compressed_bytes = NULL;
  unsigned char* uncompressed_bytes = NULL;
  unsigned int compressed_buf_len;
  int ret;

  // Get members of QatCompressorDecompressorJNI
  jobject clazz = (*env)->GetStaticObjectField(env,thisj, QatCompressorDecompressorJNI_clazz);
  jobject compressed_direct_buf = (*env)->GetObjectField(env,thisj, QatCompressorDecompressorJNI_compressedDirectBuf);
  jint compressed_direct_buf_len = (*env)->GetIntField(env,thisj, QatCompressorDecompressorJNI_compressedDirectBufLen);
  jobject uncompressed_direct_buf = (*env)->GetObjectField(env,thisj, QatCompressorDecompressorJNI_uncompressedDirectBuf);
  unsigned int uncompressed_direct_buf_len = (*env)->GetIntField(env, thisj, QatCompressorDecompressorJNI_directBufferSize);

  // Get the input direct buffer
  LOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");
  compressed_bytes = (const unsigned char*)(*env)->GetDirectBufferAddress(env, compressed_direct_buf);
  UNLOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");

  if (compressed_bytes == 0) {
    return (jint)0;
  }

  // Get the output direct buffer
  LOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");
  uncompressed_bytes = (unsigned char *)(*env)->GetDirectBufferAddress(env, uncompressed_direct_buf);
  UNLOCK_CLASS(env, clazz, "QatCompressorDecompressorJNI");

  if (uncompressed_bytes == 0) {
    return (jint)0;
  }

  compressed_buf_len = compressed_direct_buf_len;
  ret = dlsym_qzDecompress(&g_qzSession, compressed_bytes, &compressed_buf_len,
        uncompressed_bytes, &uncompressed_direct_buf_len);

  if (ret != QZ_OK) {
    THROW(env, "java/lang/InternalError", "Could not decompress data, return " + ret);
  }

  (*env)->SetIntField(env, thisj, QatCompressorDecompressorJNI_compressedDirectBufLen, 0);

  return (jint)uncompressed_direct_buf_len;
}

JNIEXPORT jstring JNICALL
Java_com_intel_qat_jni_QatCompressorDecompressorJNI_getLibraryName(JNIEnv *env, jclass class) {
#ifdef UNIX
  if (dlsym_qzCompress) {
    Dl_info dl_info;
    if(dladdr(
        dlsym_qzCompress,
        &dl_info)) {
      return (*env)->NewStringUTF(env, dl_info.dli_fname);
    }
  }

  return (*env)->NewStringUTF(env, LUCENE_QAT_LIBRARY);
#endif

#ifdef WINDOWS
  LPWSTR filename = NULL;
  GetLibraryName(dlsym_qzCompress, &filename);
  if (filename != NULL) {
    return (*env)->NewString(env, filename, (jsize) wcslen(filename));
  } else {
    return (*env)->NewStringUTF(env, "Unavailable");
  }
#endif
}

JNIEXPORT jobject JNICALL
Java_com_intel_qat_jni_QatCompressorDecompressorJNI_nativeAllocateBB(JNIEnv *env,
 jobject obj, jlong capacity, jboolean numa, jboolean force_pinned){
//void *buf = dlsym_qat_malloc(capacity);
//printf("compressor: DBB address is 0x%lx\n",(unsigned long)buf);
//fflush(stdout);
//  return (*env)->NewDirectByteBuffer(env, buf, capacity);
  return (*env)->NewDirectByteBuffer(env, dlsym_qzMalloc(capacity, numa, force_pinned), capacity);
}

#endif //define LUCENE_QAT_LIBRARY
