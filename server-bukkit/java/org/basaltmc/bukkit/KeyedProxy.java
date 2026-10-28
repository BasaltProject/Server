package org.basaltmc.bukkit;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import org.bukkit.Keyed;
import org.bukkit.NamespacedKey;

/** A registry entry of an interface type that only knows its key; anything else asked is reported once. */
final class KeyedProxy implements InvocationHandler {
    private final Class<?> type;
    private final NamespacedKey key;
    private final int ordinal;
    private final java.util.Map<String, Object[]> built;

    private KeyedProxy(Class<?> type, NamespacedKey key, int ordinal, java.util.Map<String, Object[]> built) {
        this.type = type;
        this.key = key;
        this.ordinal = ordinal;
        this.built = built;
    }

    static <T extends Keyed> T of(Class<T> type, NamespacedKey key, int ordinal) {
        return of(type, key, ordinal, java.util.Map.of());
    }

    /** An entry whose getters answer with the value a builder was given under the same name. */
    @SuppressWarnings("unchecked")
    static <T extends Keyed> T of(Class<T> type, NamespacedKey key, int ordinal,
            java.util.Map<String, Object[]> built) {
        return (T) Proxy.newProxyInstance(type.getClassLoader(), new Class<?>[] {type},
            new KeyedProxy(type, key, ordinal, built));
    }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        switch (method.getName()) {
            case "getKey", "getKeyOrNull", "getKeyOrThrow":
                if (args == null || args.length == 0) {
                    return key;
                }
                break;
            case "key":
                if (args == null || args.length == 0) {
                    return net.kyori.adventure.key.Key.key(key.getNamespace(), key.getKey());
                }
                break;
            case "ordinal":
                return ordinal;
            case "getValue":
                if (type == org.bukkit.map.MapCursor.Type.class) {
                    return (byte) ordinal;
                }
                break;
            case "name":
                return key.getKey().toUpperCase(java.util.Locale.ROOT);
            case "toString":
                return type.getSimpleName() + "{" + key + "}";
            case "hashCode":
                return key.hashCode() * 31 + type.hashCode();
            case "equals":
                return proxy == args[0] || (args[0] != null && Proxy.isProxyClass(args[0].getClass())
                    && Proxy.getInvocationHandler(args[0]) instanceof KeyedProxy other
                    && other.type == type && other.key.equals(key));
            case "compareTo":
                return ordinal - ((Number) args[0].getClass().getMethod("ordinal").invoke(args[0])).intValue();
            default:
                break;
        }
        Object[] given = built.get(method.getName());
        if (given != null && given.length == 1 && (args == null || args.length == 0)
            && (method.getReturnType().isInstance(given[0])
                || method.getReturnType().isPrimitive() && given[0] != null)) {
            return given[0];
        }
        Object fact = KeyedFacts.answer(type, key, method, args);
        if (fact != KeyedFacts.kNone) {
            return fact;
        }
        if (method.isDefault()) {
            return InvocationHandler.invokeDefault(proxy, method, args);
        }
        throw Unsupported.hit(type.getSimpleName() + "#" + method.getName());
    }
}
