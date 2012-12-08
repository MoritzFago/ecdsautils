/*
  Copyright (c) 2012, Nils Schneider <nils@nilsschneider.net>
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libuecc/ecc.h>

#include "hexutil.h"

void random_bytes(char *buffer, size_t len) {
  int fd;
  size_t read_bytes = 0;

  fd = open("/dev/random", O_RDONLY);

  if (fd < 0) {
    error(1, errno, "Can't open /dev/random");
  }

  while (read_bytes < len) {
    ssize_t ret = read(fd, buffer + read_bytes, len - read_bytes);

    if (ret < 0) {
      if (errno == EINTR)
        continue;

      error(1, errno, "Error while reading random bytes");
    }

    read_bytes += ret;
  }

  close(fd);
}

void main(int argc, char *argv[]) {
  ecc_int_256 secret, hash, k, krecip, r, s, tmp;
  ecc_25519_work kG;

  if (argc != 3)
    error(1, 0, "Usage: %s secret hash", argv[0]);

  if (!parsehex(argv[1], secret.p, 32))
    error(1, 0, "Error while reading secret");

  if (!parsehex(argv[2], hash.p, 32))
    error(1, 0, "Error while reading hash");

  while (1) {
    // genarate random k
    random_bytes(k.p, 32);
    ecc_25519_gf_sanitize_secret(&k, &k);

    // calculate k^(-1)
    ecc_25519_gf_recip(&krecip, &k);

    // calculate kG = k * base point
    ecc_25519_scalarmult_base(&kG, &k);

    // store x coordinate of kG in r
    ecc_25519_store_xy(&r, &tmp, &kG);

    if (ecc_25519_gf_is_zero(&r))
      continue;

    // tmp = r * secret
    ecc_25519_gf_mult(&tmp, &r, &secret);

    // s = hash + tmp = hash + r * secret
    ecc_25519_gf_add(&s, &hash, &tmp);

    // tmp = krecip * s = k^(-1) * s
    ecc_25519_gf_mult(&tmp, &krecip, &s);

    // mod n (order of G)
    ecc_25519_gf_reduce(&s, &tmp);

    if (ecc_25519_gf_is_zero(&s))
      continue;

    break;
  }

  hexdump(stdout, r.p, 32);
  hexdump(stdout, s.p, 32);
  puts("");
}