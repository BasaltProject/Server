package org.basaltmc.bukkit;

import io.papermc.paper.plugin.lifecycle.event.handler.LifecycleEventHandler;
import io.papermc.paper.registry.RegistryAccess;
import io.papermc.paper.registry.RegistryBuilderFactory;
import io.papermc.paper.registry.RegistryKey;
import io.papermc.paper.registry.TypedKey;
import io.papermc.paper.registry.event.BasaltRegistryEventTypeProvider.Recorded;
import io.papermc.paper.registry.event.RegistryComposeEvent;
import io.papermc.paper.registry.event.RegistryEntryAddEvent;
import io.papermc.paper.registry.event.WritableRegistry;
import io.papermc.paper.registry.tag.Tag;
import io.papermc.paper.registry.tag.TagKey;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;
import net.kyori.adventure.key.Key;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;

/** Runs the registry handlers a bootstrapper registered, against the registries plugins see, at bootstrap time. */
public final class RegistryLifecycle {
    private RegistryLifecycle() {}

    /** The calls a handler made on a builder, kept as what the entry it describes answers for the same names. */
    private static final class Capture implements InvocationHandler {
        private final Class<?> type;
        private final Map<String, Object[]> calls = new LinkedHashMap<>();
        private Object self;

        Capture(Class<?> type) {
            this.type = type;
        }

        Object builder() {
            if (self == null) {
                self = Proxy.newProxyInstance(type.getClassLoader(), new Class<?>[] {type}, this);
            }
            return self;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) {
            switch (method.getName()) {
                case "toString":
                    return type.getSimpleName() + calls.keySet();
                case "hashCode":
                    return System.identityHashCode(proxy);
                case "equals":
                    return proxy == args[0];
                default:
                    break;
            }
            calls.put(method.getName(), args == null ? new Object[0] : args);
            Class<?> returned = method.getReturnType();
            if (returned.isAssignableFrom(type)) {
                return proxy;
            }
            if (returned == boolean.class) {
                return false;
            }
            if (returned.isPrimitive() && returned != void.class) {
                return returned == double.class ? 0.0 : returned == float.class ? 0f : returned == long.class ? 0L : 0;
            }
            return null;
        }
    }

    private static String name(Recorded<?> one) {
        return one.registry().key().value();
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static void run(List<Recorded<?>> handlers, String owner) {
        for (Recorded<?> one : handlers.stream().filter(Recorded::compose)
                .sorted(Comparator.comparingInt(Recorded::priority)).toList()) {
            try {
                compose((Recorded) one, owner);
            } catch (RuntimeException failure) {
                Natives.log(3, "Bukkit", owner + " failed in a " + one.event() + " handler: " + failure);
            }
        }
        for (Recorded<?> one : handlers.stream().filter(r -> !r.compose())
                .sorted(Comparator.comparingInt(Recorded::priority)).toList()) {
            try {
                entries((Recorded) one, owner);
            } catch (RuntimeException failure) {
                Natives.log(3, "Bukkit", owner + " failed in a " + one.event() + " handler: " + failure);
            }
        }
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static BasaltRegistry<Keyed> live(RegistryKey<?> key) {
        return (BasaltRegistry<Keyed>) (BasaltRegistry) RegistryAccess.registryAccess().getRegistry((RegistryKey) key);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static void compose(Recorded<Object> one, String owner) {
        BasaltRegistry<Keyed> registry = live(one.registry());
        Class<?> entryType = org.basaltmc.bukkit.gen.RegistryTypes.kByName.get(name(one));
        WritableRegistry writable = new WritableRegistry() {
            @Override
            public void registerWith(TypedKey key, Consumer factory) {
                NamespacedKey id = new NamespacedKey(key.namespace(), key.value());
                if (registry.get(id) != null) {
                    throw new IllegalStateException("Duplicate registry entry " + id + " in " + name(one));
                }
                Capture capture = new Capture(one.builder());
                factory.accept(new RegistryBuilderFactory() {
                    @Override
                    public io.papermc.paper.registry.RegistryBuilder empty() {
                        return (io.papermc.paper.registry.RegistryBuilder) capture.builder();
                    }

                    @Override
                    public io.papermc.paper.registry.RegistryBuilder copyFrom(TypedKey from) {
                        return (io.papermc.paper.registry.RegistryBuilder) capture.builder();
                    }
                });
                Keyed made;
                if (entryType == org.bukkit.enchantments.Enchantment.class) {
                    made = new org.basaltmc.bukkit.item.BasaltEnchantment(id);
                } else if (entryType != null && entryType.isInterface() && Keyed.class.isAssignableFrom(entryType)) {
                    made = KeyedProxy.of((Class<? extends Keyed>) entryType, id, -1, capture.calls);
                } else {
                    throw Unsupported.hit("registering into " + name(one));
                }
                registry.addEntry(made);
                Natives.log(2, "Bukkit", owner + " registered " + id + " in " + name(one)
                    + ": plugins see it, the server has no data for it");
            }
        };
        RegistryComposeEvent event = new RegistryComposeEvent() {
            @Override
            public WritableRegistry registry() {
                return writable;
            }

            @Override
            public Tag getOrCreateTag(TagKey key) {
                throw Unsupported.hit("RegistryComposeEvent#getOrCreateTag");
            }

            @Override
            public RegistryKey registryKey() {
                return one.registry();
            }
        };
        ((LifecycleEventHandler) one.handler()).run(event);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static void entries(Recorded<Object> one, String owner) {
        BasaltRegistry<Keyed> registry = live(one.registry());
        boolean warned = false;
        for (Keyed entry : registry.stream().toList()) {
            TypedKey key = TypedKey.create(one.registry(), Key.key(entry.getKey().getNamespace(), entry.getKey().getKey()));
            if (!one.accepts(key)) {
                continue;
            }
            Capture capture = new Capture(one.builder());
            RegistryEntryAddEvent event = new RegistryEntryAddEvent() {
                @Override
                public io.papermc.paper.registry.RegistryBuilder builder() {
                    return (io.papermc.paper.registry.RegistryBuilder) capture.builder();
                }

                @Override
                public TypedKey key() {
                    return key;
                }

                @Override
                public Tag getOrCreateTag(TagKey tag) {
                    throw Unsupported.hit("RegistryEntryAddEvent#getOrCreateTag");
                }

                @Override
                public RegistryKey registryKey() {
                    return one.registry();
                }
            };
            ((LifecycleEventHandler) one.handler()).run(event);
            if (!capture.calls.isEmpty() && !warned) {
                warned = true;
                Natives.log(2, "Bukkit", owner + " changed entries of " + name(one)
                    + " in a registry handler; the server keeps its own data for them, so the changes do not apply");
            }
        }
    }
}
