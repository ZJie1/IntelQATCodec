/**
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

package org.apache.hadoop.hive.ql.io.orc;

import org.apache.hadoop.hive.shims.HadoopShims;
import org.apache.hadoop.hive.shims.ShimLoader;
import org.apache.hadoop.io.compress.qat.QatCompressor;
import org.apache.hadoop.io.compress.qat.QatDecompressor;

import javax.annotation.Nullable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.EnumSet;

public class QATCodec implements CompressionCodec, DirectDecompressionCodec{

  private Boolean direct = null;
  private int bufferSize;

  public QATCodec(int bufferSize) {
    this.bufferSize = bufferSize;
  }

  @Override
  public boolean isAvailable() {
    if (direct == null) {
      // see nowrap option in new Inflater(boolean) which disables zlib headers
      try {
        if (ShimLoader.getHadoopShims().getDirectDecompressor(HadoopShims.DirectCompressionType.QAT)
          != null) {
          direct = Boolean.valueOf(true);
        } else {
          direct = Boolean.valueOf(false);
        }
      } catch (UnsatisfiedLinkError ule) {
        direct = Boolean.valueOf(false);
      }
    }
    return direct.booleanValue();
  }

  @Override
  public void directDecompress(
    ByteBuffer in,
    ByteBuffer out) throws IOException {
    HadoopShims.DirectDecompressorShim decompressShim = ShimLoader.getHadoopShims()
      .getDirectDecompressor(HadoopShims.DirectCompressionType.QAT);
    decompressShim.decompress(in, out);
    out.flip(); // flip for read
  }

  @Override
  public boolean compress(
    ByteBuffer in,
    ByteBuffer out,
    ByteBuffer overflow) throws IOException {
    QatCompressor compressor = new QatCompressor(bufferSize);
    int length = in.remaining();
    compressor.setInput(in.array(), in.arrayOffset() + in.position(), length);
    compressor.finish();
    int outSize = 0;
    int offset = out.arrayOffset() + out.position();
    while (!compressor.finished() && (length > outSize)) {
      int size = compressor.compress(out.array(), offset, out.remaining());
      out.position(size + out.position());
      outSize += size;
      offset += size;
      // if we run out of space in the out buffer, use the overflow
      if (out.remaining() == 0) {
        if (overflow == null) {
          compressor.end();
          return false;
        }
        out = overflow;
        offset = out.arrayOffset() + out.position();
      }
    }
    compressor.end();
    return length > outSize;
  }

  @Override
  public void decompress(
    ByteBuffer in,
    ByteBuffer out) throws IOException {
    if (in.isDirect() && out.isDirect()) {
      directDecompress(in, out);
      return;
    }

    QatDecompressor decompressor = new QatDecompressor(bufferSize);
    decompressor.setInput(in.array(), in.arrayOffset() + in.position(), in.remaining());
    while (!(decompressor.finished() || decompressor.needsDictionary() ||
      decompressor.needsInput())) {
      int count =
        decompressor.decompress(out.array(), out.arrayOffset() + out.position(), out.remaining());
      out.position(count + out.position());
    }
    out.flip();
    decompressor.end();
    in.position(in.limit());
  }

  @Override
  public CompressionCodec modify(@Nullable EnumSet<Modifier> modifiers) {
    // TODO: support modifiers
    return this;
  }
}
