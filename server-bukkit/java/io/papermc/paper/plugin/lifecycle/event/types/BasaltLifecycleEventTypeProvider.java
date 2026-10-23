package io.papermc.paper.plugin.lifecycle.event.types;

import io.papermc.paper.plugin.bootstrap.BootstrapContext;
import io.papermc.paper.plugin.lifecycle.event.LifecycleEvent;
import io.papermc.paper.plugin.lifecycle.event.LifecycleEventOwner;
import io.papermc.paper.plugin.lifecycle.event.handler.LifecycleEventHandler;
import io.papermc.paper.plugin.lifecycle.event.handler.configuration.LifecycleEventHandlerConfiguration;
import io.papermc.paper.plugin.lifecycle.event.handler.configuration.MonitorLifecycleEventHandlerConfiguration;
import io.papermc.paper.plugin.lifecycle.event.handler.configuration.PrioritizedLifecycleEventHandlerConfiguration;
import io.papermc.paper.plugin.lifecycle.event.registrar.ReloadableRegistrarEvent;
import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.tag.TagKey;
import org.basaltmc.bukkit.Unsupported;

/** The lifecycle event types plugins name; the server runs the ones it has a use for and lists the rest as unused. */
public final class BasaltLifecycleEventTypeProvider implements LifecycleEventTypeProvider {
    /** What a plugin asked for: a handler on an event type, with how it wants to be ordered. */
    public static final class Handler<O extends LifecycleEventOwner> implements
            PrioritizedLifecycleEventHandlerConfiguration<O>, MonitorLifecycleEventHandlerConfiguration<O> {
        private final String event;
        private final LifecycleEventHandler<? extends LifecycleEvent> handler;
        private int priority;
        private boolean monitor;

        Handler(String event, LifecycleEventHandler<? extends LifecycleEvent> handler) {
            this.event = event;
            this.handler = handler;
        }

        public String event() {
            return event;
        }

        @SuppressWarnings("unchecked")
        public <E extends LifecycleEvent> LifecycleEventHandler<E> handler() {
            return (LifecycleEventHandler<E>) handler;
        }

        public int priority() {
            return priority;
        }

        public boolean monitors() {
            return monitor;
        }

        @Override
        public Handler<O> priority(int priority) {
            this.priority = priority;
            return this;
        }

        @Override
        public Handler<O> monitor() {
            this.monitor = true;
            return this;
        }
    }

    private static final class Ordered<O extends LifecycleEventOwner, E extends LifecycleEvent>
            implements LifecycleEventType.Prioritizable<O, E> {
        private final String name;

        Ordered(String name) {
            this.name = name;
        }

        @Override
        public String name() {
            return name;
        }

        @Override
        public PrioritizedLifecycleEventHandlerConfiguration<O> newHandler(LifecycleEventHandler<? super E> handler) {
            return new Handler<O>(name, handler);
        }
    }

    private static final class Watching<O extends LifecycleEventOwner, E extends LifecycleEvent>
            implements LifecycleEventType.Monitorable<O, E> {
        private final String name;

        Watching(String name) {
            this.name = name;
        }

        @Override
        public String name() {
            return name;
        }

        @Override
        public MonitorLifecycleEventHandlerConfiguration<O> newHandler(LifecycleEventHandler<? super E> handler) {
            return new Handler<O>(name, handler);
        }
    }

    @Override
    public <O extends LifecycleEventOwner, E extends LifecycleEvent> LifecycleEventType.Monitorable<O, E> monitor(
            String name, Class<? extends O> ownerType) {
        return new Watching<>(name);
    }

    @Override
    public <O extends LifecycleEventOwner, E extends LifecycleEvent> LifecycleEventType.Prioritizable<O, E> prioritized(
            String name, Class<? extends O> ownerType) {
        return new Ordered<>(name);
    }

    @Override
    public TagEventTypeProvider tagProvider() {
        return new TagEventTypeProvider() {
            @Override
            public <T> LifecycleEventType.Prioritizable<BootstrapContext,
                    ReloadableRegistrarEvent<io.papermc.paper.tag.PreFlattenTagRegistrar<T>>>
                    preFlatten(RegistryKey<T> registryKey) {
                return new Ordered<>("tags/pre_flatten/" + registryKey.key());
            }

            @Override
            public <T> LifecycleEventType.Prioritizable<BootstrapContext,
                    ReloadableRegistrarEvent<io.papermc.paper.tag.PostFlattenTagRegistrar<T>>>
                    postFlatten(RegistryKey<T> registryKey) {
                return new Ordered<>("tags/post_flatten/" + registryKey.key());
            }
        };
    }
}
