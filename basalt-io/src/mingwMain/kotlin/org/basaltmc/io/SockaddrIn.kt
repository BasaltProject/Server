@file:OptIn(ExperimentalForeignApi::class)

package org.basaltmc.io

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.convert
import platform.posix.AF_INET
import platform.posix.sockaddr_in

/**
 * Windows' `sockaddr_in` has no BSD-style `sin_len` either; `sin_family` is a 16-bit `ADDRESS_FAMILY`.
 * Unverified: this repo builds only macOS targets locally, and CI is not currently running.
 * Written from the known Winsock `sockaddr_in` layout, not guessed blind: confirm on the next
 * Windows CI run.
 */
internal actual fun sockaddr_in.setAddressFamily() {
    sin_family = AF_INET.convert()
}
