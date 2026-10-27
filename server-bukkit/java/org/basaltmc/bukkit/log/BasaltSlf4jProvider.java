package org.basaltmc.bukkit.log;

import java.util.concurrent.ConcurrentHashMap;
import org.slf4j.ILoggerFactory;
import org.slf4j.IMarkerFactory;
import org.slf4j.Logger;
import org.slf4j.event.Level;
import org.slf4j.helpers.BasicMDCAdapter;
import org.slf4j.helpers.BasicMarkerFactory;
import org.slf4j.helpers.LegacyAbstractLogger;
import org.slf4j.helpers.MessageFormatter;
import org.slf4j.spi.MDCAdapter;
import org.slf4j.spi.SLF4JServiceProvider;

/** Sends SLF4J to the server log, tagged with the plugin whose code asked for the logger. */
public final class BasaltSlf4jProvider implements SLF4JServiceProvider {
    private final ConcurrentHashMap<String, Logger> loggers = new ConcurrentHashMap<>();
    private final IMarkerFactory markers = new BasicMarkerFactory();
    private final MDCAdapter mdc = new BasicMDCAdapter();
    private final ILoggerFactory factory = name -> {
        String tag = LogTags.ofCaller(name.substring(name.lastIndexOf('.') + 1));
        return org.basaltmc.bukkit.facade.Memo.get(loggers, tag + "/" + name, key -> new Slf4jLogger(name, tag));
    };

    @Override
    public ILoggerFactory getLoggerFactory() {
        return factory;
    }

    @Override
    public IMarkerFactory getMarkerFactory() {
        return markers;
    }

    @Override
    public MDCAdapter getMDCAdapter() {
        return mdc;
    }

    @Override
    public String getRequestedApiVersion() {
        return "2.0.99";
    }

    @Override
    public void initialize() {}

    private static final class Slf4jLogger extends LegacyAbstractLogger {
        private final String tag;

        Slf4jLogger(String name, String tag) {
            this.name = name;
            this.tag = tag;
        }

        @Override
        public boolean isTraceEnabled() {
            return false;
        }

        @Override
        public boolean isDebugEnabled() {
            return false;
        }

        @Override
        public boolean isInfoEnabled() {
            return true;
        }

        @Override
        public boolean isWarnEnabled() {
            return true;
        }

        @Override
        public boolean isErrorEnabled() {
            return true;
        }

        @Override
        protected String getFullyQualifiedCallerName() {
            return null;
        }

        @Override
        protected void handleNormalizedLoggingCall(Level level, org.slf4j.Marker marker, String message,
                Object[] arguments, Throwable thrown) {
            int mapped = level == Level.ERROR ? 3 : level == Level.WARN ? 2 : level == Level.INFO ? 1 : 0;
            if (mapped > 0) {
                LogTags.write(mapped, tag, MessageFormatter.arrayFormat(message, arguments).getMessage(), thrown);
            }
        }
    }
}
