package io.papermc.paper.registry.event;

import io.papermc.paper.plugin.bootstrap.BootstrapContext;
import io.papermc.paper.plugin.lifecycle.event.handler.LifecycleEventHandler;
import io.papermc.paper.plugin.lifecycle.event.types.LifecycleEventType;
import io.papermc.paper.registry.RegistryBuilder;
import io.papermc.paper.registry.TypedKey;
import io.papermc.paper.registry.event.type.RegistryEntryAddConfiguration;
import io.papermc.paper.registry.event.type.RegistryEntryAddEventType;
import java.util.function.Predicate;

/** Accepts the registry events plugins ask for in a bootstrapper; the server has no use for them and says so. */
public final class BasaltRegistryEventTypeProvider implements RegistryEventTypeProvider {
    /** A handler the server will not run: it only remembers which event it was for. */
    public static final class Unused<T> implements RegistryEntryAddConfiguration<T> {
        private final String event;

        Unused(String event) {
            this.event = event;
        }

        public String event() {
            return event;
        }

        @Override
        public RegistryEntryAddConfiguration<T> filter(Predicate<TypedKey<T>> filter) {
            return this;
        }

        @Override
        public RegistryEntryAddConfiguration<T> priority(int priority) {
            return this;
        }

        @Override
        public RegistryEntryAddConfiguration<T> monitor() {
            return this;
        }
    }

    @Override
    public <T, B extends RegistryBuilder<T>> RegistryEntryAddEventType<T, B> registryEntryAdd(
            RegistryEventProvider<T, B> provider) {
        String name = "registry/entry_add/" + provider.registryKey().key();
        return new RegistryEntryAddEventType<>() {
            @Override
            public String name() {
                return name;
            }

            @Override
            public RegistryEntryAddConfiguration<T> newHandler(
                    LifecycleEventHandler<? super RegistryEntryAddEvent<T, B>> handler) {
                return new Unused<>(name);
            }
        };
    }

    @Override
    public <T, B extends RegistryBuilder<T>> LifecycleEventType.Prioritizable<BootstrapContext,
            RegistryComposeEvent<T, B>> registryCompose(RegistryEventProvider<T, B> provider) {
        String name = "registry/compose/" + provider.registryKey().key();
        return new LifecycleEventType.Prioritizable<>() {
            @Override
            public String name() {
                return name;
            }

            @Override
            public RegistryEntryAddConfiguration<T> newHandler(
                    LifecycleEventHandler<? super RegistryComposeEvent<T, B>> handler) {
                return new Unused<>(name);
            }
        };
    }
}
