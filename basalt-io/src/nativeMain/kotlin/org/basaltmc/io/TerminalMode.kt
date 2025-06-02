package org.basaltmc.io

/**
 * Puts the controlling terminal into "raw" mode, where a keypress arrives as it happens instead of
 * the terminal buffering a whole line and handling editing itself.
 *
 * That buffering is exactly what makes an arrow key useless: in the default (canonical) mode the
 * terminal hands over a finished line and echoes anything it does not itself understand, so the
 * escape sequence an arrow sends is printed as `^[[A` rather than reaching the program. Reading a
 * history requires taking editing over, and taking editing over requires this.
 *
 * Returns whether the mode was actually changed. It is `false` whenever stdin is not a terminal at
 * all (piped input, a `systemd` unit, a container without a TTY), which is not a failure: the
 * caller falls back to plain line reading, which is all those cases can support anyway.
 */
public expect fun enableTerminalRawMode(): Boolean

/** Puts the terminal back exactly as it was found. Safe to call even if raw mode was never entered. */
public expect fun restoreTerminalMode()

/** One byte from stdin, or -1 at end of input. In raw mode this returns as soon as a key is pressed. */
public expect fun readTerminalByte(): Int
