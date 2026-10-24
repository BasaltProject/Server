package org.basaltmc.bukkit.log;

import java.net.URI;
import java.util.concurrent.ConcurrentHashMap;
import org.apache.logging.log4j.message.MessageFactory;
import org.apache.logging.log4j.message.ParameterizedMessageFactory;
import org.apache.logging.log4j.spi.ExtendedLogger;
import org.apache.logging.log4j.spi.LoggerContext;
import org.apache.logging.log4j.spi.LoggerContextFactory;

/** Sends every log4j logger to the server log, tagged with the plugin whose class loader asked for it. */
public final class BasaltLog4jFactory implements LoggerContextFactory {
    private static final LoggerContext CONTEXT = new Context();

    @Override
    public LoggerContext getContext(String fqcn, ClassLoader loader, Object externalContext, boolean currentContext) {
        return CONTEXT;
    }

    @Override
    public LoggerContext getContext(String fqcn, ClassLoader loader, Object externalContext, boolean currentContext,
            URI configLocation, String name) {
        return CONTEXT;
    }

    @Override
    public void removeContext(LoggerContext context) {}

    private static final class Context implements LoggerContext {
        private final ConcurrentHashMap<String, ExtendedLogger> loggers = new ConcurrentHashMap<>();

        @Override
        public Object getExternalContext() {
            return null;
        }

        @Override
        public ExtendedLogger getLogger(String name) {
            return getLogger(name, ParameterizedMessageFactory.INSTANCE);
        }

        @Override
        public ExtendedLogger getLogger(String name, MessageFactory factory) {
            return org.basaltmc.bukkit.facade.Memo.get(loggers, name, key -> new BasaltLog4jLogger(key,
                LogTags.ofCaller(key.substring(key.lastIndexOf('.') + 1)), factory));
        }

        @Override
        public boolean hasLogger(String name) {
            return loggers.containsKey(name);
        }

        @Override
        public boolean hasLogger(String name, MessageFactory factory) {
            return loggers.containsKey(name);
        }

        @Override
        public boolean hasLogger(String name, Class<? extends MessageFactory> factory) {
            return loggers.containsKey(name);
        }
    }
}
