package org.basaltmc.bukkit.log;

import java.net.URI;
import java.util.ArrayList;
import org.apache.logging.log4j.Level;
import org.apache.logging.log4j.core.LogEvent;
import org.apache.logging.log4j.core.appender.AbstractAppender;
import org.apache.logging.log4j.core.config.DefaultConfiguration;
import org.apache.logging.log4j.core.config.Property;
import org.apache.logging.log4j.spi.LoggerContext;
import org.apache.logging.log4j.spi.LoggerContextFactory;

/** A real Log4j context with the native server log as its default appender. */
public final class BasaltLog4jFactory implements LoggerContextFactory {
    private static final org.apache.logging.log4j.core.LoggerContext CONTEXT = createContext();

    private static org.apache.logging.log4j.core.LoggerContext createContext() {
        org.apache.logging.log4j.core.LoggerContext context =
                new org.apache.logging.log4j.core.LoggerContext("Basalt");
        DefaultConfiguration configuration = new DefaultConfiguration();
        for (String name : new ArrayList<>(configuration.getRootLogger().getAppenders().keySet())) {
            configuration.getRootLogger().removeAppender(name);
        }
        AbstractAppender appender = new AbstractAppender("Basalt", null, null, false, Property.EMPTY_ARRAY) {
            @Override public void append(LogEvent event) {
                Level level = event.getLevel();
                int mapped = level.isMoreSpecificThan(Level.ERROR) ? 3 : level.isMoreSpecificThan(Level.WARN) ? 2
                        : level.isMoreSpecificThan(Level.INFO) ? 1 : 0;
                String name = event.getLoggerName();
                String fallback = name == null || name.isEmpty() ? "Log4j"
                        : name.substring(name.lastIndexOf(".") + 1);
                LogTags.write(mapped, LogTags.ofCaller(fallback), event.getMessage().getFormattedMessage(),
                        event.getThrown() != null ? event.getThrown() : event.getMessage().getThrowable());
            }
        };
        appender.start();
        configuration.addAppender(appender);
        configuration.getRootLogger().addAppender(appender, null, null);
        configuration.getRootLogger().setLevel(Level.INFO);
        context.start(configuration);
        return context;
    }

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
}
