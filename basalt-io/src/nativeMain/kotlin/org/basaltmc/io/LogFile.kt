package org.basaltmc.io

import org.basaltmc.common.logging.LogOutput
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * Mirrors everything the console shows into `<root>/latest.log`, and archives the previous run's
 * file on the way in.
 *
 * `latest.log` sits alone at the top of [root] while archives go one level down, in
 * [ARCHIVE_DIRECTORY]: the file anyone actually wants to open is the current one, and burying it in
 * a list of hundreds of dated archives is the exact annoyance this layout avoids.
 *
 * Archives are gzipped and named `yyyy-MM-dd-N.log.gz`, where `N` counts runs on the same date, so
 * restarting several times a day never overwrites an earlier log. The next free `N` is found by
 * probing rather than listing, which needs no directory read and cannot race with itself.
 *
 * Nothing here is buffered: a line is appended as it is logged. Log volume is nowhere near an I/O
 * concern next to chunk generation, and a crash keeping every line written up to the moment it
 * happened is worth far more here than saved syscalls.
 */
public class LogFile private constructor(
    private val path: String,
) {
    private val mutex = NativeMutex()

    private fun append(line: String) {
        mutex.withLock { FileIO.appendText(path, line + "\n") }
    }

    public companion object {
        public const val ARCHIVE_DIRECTORY: String = "archive"
        public const val CURRENT_FILE: String = "latest.log"

        /**
         * Rotates the previous run's log, applies [keepDays], and starts mirroring into a fresh
         * `latest.log`, opening it with [header]. [keepDays] of 0 or less keeps archives forever.
         */
        @OptIn(ExperimentalTime::class)
        public fun install(
            root: String,
            keepDays: Int,
            header: String? = null,
        ): LogFile {
            val archiveRoot = "$root/$ARCHIVE_DIRECTORY"
            FileIO.createDirectories(archiveRoot)
            val path = "$root/$CURRENT_FILE"

            val previous = FileIO.readBytes(path)
            if (previous != null && previous.isNotEmpty()) {
                FileIO.writeBytes(nextArchivePath(archiveRoot, today()), Gzip.compress(previous))
            }
            FileIO.delete(path)

            if (keepDays > 0) pruneArchives(archiveRoot, keepDays)

            val file = LogFile(path)
            // The banner and version line reach the console through plain printing, before any
            // logger exists; without this the log would open mid-startup with no idea what built it.
            header?.let(file::append)
            LogOutput.addSink(file::append)
            return file
        }

        /**
         * Deletes archives whose own date is more than [keepDays] before today, read straight from
         * the file name. Dates are ISO-8601, so a plain string comparison against the cutoff orders
         * them correctly and needs no calendar arithmetic beyond the cutoff itself.
         */
        internal fun pruneArchives(
            archiveRoot: String,
            keepDays: Int,
        ) {
            val cutoff = daysBeforeToday(keepDays)
            for (name in FileIO.list(archiveRoot)) {
                if (!name.endsWith(".log.gz") || name.length < 10) continue
                if (name.substring(0, 10) < cutoff) FileIO.delete("$archiveRoot/$name")
            }
        }

        private fun nextArchivePath(
            archiveRoot: String,
            date: String,
        ): String {
            var index = 1
            while (FileIO.exists("$archiveRoot/$date-$index.log.gz")) index++
            return "$archiveRoot/$date-$index.log.gz"
        }

        @OptIn(ExperimentalTime::class)
        private fun today(): String = Clock.System.now().toString().substring(0, 10)

        /** [days] before today as `yyyy-MM-dd`, doing the calendar arithmetic on the instant itself. */
        @OptIn(ExperimentalTime::class)
        internal fun daysBeforeToday(days: Int): String =
            Clock.System
                .now()
                .minus(kotlin.time.Duration.parse("${days}d"))
                .toString()
                .substring(0, 10)
    }
}
