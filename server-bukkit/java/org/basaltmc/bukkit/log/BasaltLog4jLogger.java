package org.basaltmc.bukkit.log;

import org.apache.logging.log4j.Level;
import org.apache.logging.log4j.Marker;
import org.apache.logging.log4j.message.Message;
import org.apache.logging.log4j.message.MessageFactory;
import org.apache.logging.log4j.spi.AbstractLogger;

final class BasaltLog4jLogger extends AbstractLogger {
    private final String tag;

    BasaltLog4jLogger(String name, String tag, MessageFactory factory) {
        super(name, factory);
        this.tag = tag;
    }

    @Override
    public Level getLevel() {
        return Level.INFO;
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, Message message, Throwable thrown) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, CharSequence message, Throwable thrown) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, Object message, Throwable thrown) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Throwable thrown) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object... params) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2, Object p3) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2, Object p3,
            Object p4) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2, Object p3,
            Object p4, Object p5) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2, Object p3,
            Object p4, Object p5, Object p6) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2, Object p3,
            Object p4, Object p5, Object p6, Object p7) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2, Object p3,
            Object p4, Object p5, Object p6, Object p7, Object p8) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public boolean isEnabled(Level level, Marker marker, String message, Object p0, Object p1, Object p2, Object p3,
            Object p4, Object p5, Object p6, Object p7, Object p8, Object p9) {
        return level.isMoreSpecificThan(Level.INFO);
    }

    @Override
    public void logMessage(String fqcn, Level level, Marker marker, Message message, Throwable thrown) {
        int mapped = level.isMoreSpecificThan(Level.ERROR) ? 3 : level.isMoreSpecificThan(Level.WARN) ? 2
            : level.isMoreSpecificThan(Level.INFO) ? 1 : 0;
        LogTags.write(mapped, tag, message.getFormattedMessage(), thrown != null ? thrown : message.getThrowable());
    }
}
