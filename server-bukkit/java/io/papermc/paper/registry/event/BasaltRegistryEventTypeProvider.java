package io.papermc.paper.registry.event;

import io.papermc.paper.plugin.bootstrap.BootstrapContext;
import io.papermc.paper.plugin.lifecycle.event.handler.LifecycleEventHandler;
import io.papermc.paper.plugin.lifecycle.event.types.LifecycleEventType;
import io.papermc.paper.registry.RegistryBuilder;
import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.TypedKey;
import io.papermc.paper.registry.event.type.RegistryEntryAddConfiguration;
import io.papermc.paper.registry.event.type.RegistryEntryAddEventType;
import java.lang.reflect.ParameterizedType;
import java.util.function.Predicate;

/** Hands out the registry event types a bootstrapper asks for; each handler is kept for the server to run. */
public final class BasaltRegistryEventTypeProvider implements RegistryEventTypeProvider {
    /** A registry handler a bootstrapper asked for, with the filter and priority it set. */
    public static final class Recorded<T> implements RegistryEntryAddConfiguration<T> {
        private final String event;
        private final boolean compose;
        private final RegistryKey<T> registry;
        private final Class<?> builder;
        private final LifecycleEventHandler<?> handler;
        private Predicate<TypedKey<T>> filter = key -> true;
        private int priority;

        Recorded(String event, boolean compose, RegistryKey<T> registry, Class<?> builder,
                LifecycleEventHandler<?> handler) {
            this.event = event;
            this.compose = compose;
            this.registry = registry;
            this.builder = builder;
            this.handler = handler;
        }

        public String event() {
            return event;
        }

        public boolean compose() {
            return compose;
        }

        public RegistryKey<T> registry() {
            return registry;
        }

        public Class<?> builder() {
            return builder;
        }

        public LifecycleEventHandler<?> handler() {
            return handler;
        }

        public boolean accepts(TypedKey<T> key) {
            return filter.test(key);
        }

        public int priority() {
            return priority;
        }

        @Override
        public RegistryEntryAddConfiguration<T> filter(Predicate<TypedKey<T>> filter) {
            this.filter = filter;
            return this;
        }

        @Override
        public RegistryEntryAddConfiguration<T> priority(int priority) {
            this.priority = priority;
            return this;
        }

        @Override
        public RegistryEntryAddConfiguration<T> monitor() {
            this.priority = Integer.MAX_VALUE;
            return this;
        }
    }

    /** The builder type of a registry, read off the generic type of the provider field it is published in. */
    private static Class<?> builderOf(RegistryEventProvider<?, ?> provider) {
        try {
            for (java.lang.reflect.Field field : RegistryEvents.class.getFields()) {
                if (field.get(null) == provider && field.getGenericType() instanceof ParameterizedType generic
                    && generic.getActualTypeArguments()[1] instanceof Class<?> builder) {
                    return builder;
                }
            }
        } catch (IllegalAccessException failure) {
            throw new IllegalStateException(failure);
        }
        return RegistryBuilder.class;
    }

    @Override
    public <T, B extends RegistryBuilder<T>> RegistryEntryAddEventType<T, B> registryEntryAdd(
            RegistryEventProvider<T, B> provider) {
        String name = "registry/entry_add/" + provider.registryKey().key();
        Class<?> builder = builderOf(provider);
        return new RegistryEntryAddEventType<>() {
            @Override
            public String name() {
                return name;
            }

            @Override
            public RegistryEntryAddConfiguration<T> newHandler(
                    LifecycleEventHandler<? super RegistryEntryAddEvent<T, B>> handler) {
                return new Recorded<>(name, false, provider.registryKey(), builder, handler);
            }
        };
    }

    @Override
    public <T, B extends RegistryBuilder<T>> LifecycleEventType.Prioritizable<BootstrapContext,
            RegistryComposeEvent<T, B>> registryCompose(RegistryEventProvider<T, B> provider) {
        String name = "registry/compose/" + provider.registryKey().key();
        Class<?> builder = builderOf(provider);
        return new LifecycleEventType.Prioritizable<>() {
            @Override
            public String name() {
                return name;
            }

            @Override
            public RegistryEntryAddConfiguration<T> newHandler(
                    LifecycleEventHandler<? super RegistryComposeEvent<T, B>> handler) {
                return new Recorded<>(name, true, provider.registryKey(), builder, handler);
            }
        };
    }
}
