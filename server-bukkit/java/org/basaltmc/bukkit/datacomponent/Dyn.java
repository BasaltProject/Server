package org.basaltmc.bukkit.datacomponent;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.BiFunction;
import java.util.function.Function;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.item.Nbt;

/** Component values and their builders as proxies over a field map, each spec knowing its stored form. */
public final class Dyn {
    private Dyn() {}

    /** What every value built here can say about itself: the stored form it goes back to. */
    public interface Value {
        Object nbt();
    }

    public static final class Spec {
        final Class<?> value;
        final Class<?> builder;
        final Map<String, Object> defaults = new LinkedHashMap<>();
        final Map<String, BiConsumer<Map<String, Object>, Object[]>> ops = new HashMap<>();
        final Map<String, BiFunction<Map<String, Object>, Object[], Object>> calls = new HashMap<>();
        Function<Map<String, Object>, Object> encode;
        Function<Object, Map<String, Object>> decode;

        public Spec(Class<?> value, Class<?> builder) {
            this.value = value;
            this.builder = builder;
        }

        public Spec def(String field, Object fallback) {
            defaults.put(field, fallback);
            return this;
        }

        public Spec add(String method, String field) {
            ops.put(method, (fields, args) -> list(fields, field).add(args[0]));
            return this;
        }

        public Spec addAll(String method, String field) {
            ops.put(method, (fields, args) -> list(fields, field).addAll((Collection<?>) args[0]));
            return this;
        }

        public Spec set(String method, String field) {
            ops.put(method, (fields, args) -> fields.put(field, args[0]));
            return this;
        }

        public Spec put(String method, String field) {
            ops.put(method, (fields, args) -> map(fields, field).put(args[0], args[1]));
            return this;
        }

        public Spec putAll(String method, String field) {
            ops.put(method, (fields, args) -> map(fields, field).putAll((Map<?, ?>) args[0]));
            return this;
        }

        public Spec op(String method, BiConsumer<Map<String, Object>, Object[]> action) {
            ops.put(method, action);
            return this;
        }

        public Spec call(String method, BiFunction<Map<String, Object>, Object[], Object> answer) {
            calls.put(method, answer);
            return this;
        }

        public Spec nbt(Function<Map<String, Object>, Object> encode, Function<Object, Map<String, Object>> decode) {
            this.encode = encode;
            this.decode = decode;
            return this;
        }

        public Object value(Map<String, Object> fields) {
            return Dyn.value(this, fields);
        }

        public Object builder() {
            return Dyn.builder(this, new LinkedHashMap<>());
        }

        public Object read(Object stored) {
            return Dyn.value(this, decode.apply(stored));
        }
    }

    @SuppressWarnings("unchecked")
    static List<Object> list(Map<String, Object> fields, String field) {
        return (List<Object>) fields.computeIfAbsent(field, key -> new ArrayList<>());
    }

    @SuppressWarnings("unchecked")
    static Map<Object, Object> map(Map<String, Object> fields, String field) {
        return (Map<Object, Object>) fields.computeIfAbsent(field, key -> new LinkedHashMap<>());
    }

    private static Map<String, Object> copy(Spec spec, Map<String, Object> fields) {
        Map<String, Object> out = new LinkedHashMap<>(spec.defaults);
        for (Map.Entry<String, Object> entry : fields.entrySet()) {
            Object held = entry.getValue();
            out.put(entry.getKey(), held instanceof List<?> list ? new ArrayList<>(list)
                : held instanceof Map<?, ?> map ? new LinkedHashMap<>(map) : held);
        }
        return out;
    }

    private static Object fallback(Class<?> type) {
        if (type == boolean.class) {
            return false;
        } else if (type == int.class) {
            return 0;
        } else if (type == float.class) {
            return 0F;
        } else if (type == long.class) {
            return 0L;
        } else if (type == double.class) {
            return 0D;
        } else if (type == List.class || type == Collection.class) {
            return List.of();
        } else if (type == Set.class) {
            return Set.of();
        } else if (type == Map.class) {
            return Map.of();
        }
        return null;
    }

    private static Object coerce(Object held, Class<?> type) {
        if (held instanceof Number number) {
            if (type == float.class || type == Float.class) {
                return number.floatValue();
            } else if (type == int.class || type == Integer.class) {
                return number.intValue();
            } else if (type == double.class || type == Double.class) {
                return number.doubleValue();
            } else if (type == long.class || type == Long.class) {
                return number.longValue();
            }
        }
        if (held instanceof List<?> list) {
            return List.copyOf(list);
        }
        return held;
    }

    public static Object value(Spec spec, Map<String, Object> fields) {
        Map<String, Object> held = copy(spec, fields);
        return Proxy.newProxyInstance(spec.value.getClassLoader(), new Class<?>[] {spec.value, Value.class},
            new ValueHandler(spec, held));
    }

    public static Object builder(Spec spec, Map<String, Object> fields) {
        return Proxy.newProxyInstance(spec.builder.getClassLoader(), new Class<?>[] {spec.builder},
            new BuilderHandler(spec, fields));
    }

    private record ValueHandler(Spec spec, Map<String, Object> fields) implements InvocationHandler {
        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            String name = method.getName();
            int count = args == null ? 0 : args.length;
            if (method.getDeclaringClass() == Object.class) {
                switch (name) {
                    case "equals":
                        return proxy == args[0] || args[0] instanceof Value other && Proxy.isProxyClass(args[0].getClass())
                            && Proxy.getInvocationHandler(args[0]) instanceof ValueHandler handler
                            && handler.spec == spec && Nbt.same(spec.encode.apply(fields), other.nbt());
                    case "hashCode":
                        return Nbt.hash(spec.encode.apply(fields));
                    default:
                        return spec.value.getSimpleName() + spec.encode.apply(fields);
                }
            }
            if (count == 0 && name.equals("nbt")) {
                return spec.encode.apply(fields);
            }
            BiFunction<Map<String, Object>, Object[], Object> answer = spec.calls.get(name);
            if (answer != null) {
                return answer.apply(fields, args == null ? new Object[0] : args);
            }
            if (count == 0 && name.equals("toBuilder") && spec.builder != null) {
                return builder(spec, copy(spec, fields));
            }
            if (method.isDefault()) {
                return InvocationHandler.invokeDefault(proxy, method, args);
            }
            if (count == 0) {
                Object held = fields.get(name);
                return held == null ? fallback(method.getReturnType()) : coerce(held, method.getReturnType());
            }
            throw Unsupported.hit(spec.value.getSimpleName() + "#" + name);
        }
    }

    private record BuilderHandler(Spec spec, Map<String, Object> fields) implements InvocationHandler {
        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            String name = method.getName();
            if (method.getDeclaringClass() == Object.class) {
                return switch (name) {
                    case "equals" -> proxy == args[0];
                    case "hashCode" -> System.identityHashCode(proxy);
                    default -> spec.builder.getSimpleName();
                };
            }
            if (name.equals("build") && (args == null || args.length == 0)) {
                return value(spec, fields);
            }
            BiConsumer<Map<String, Object>, Object[]> op = spec.ops.get(name);
            if (op != null) {
                op.accept(fields, args);
                return proxy;
            }
            if (method.isDefault()) {
                return InvocationHandler.invokeDefault(proxy, method, args);
            }
            if (args != null && args.length == 1) {
                fields.put(name, args[0]);
                return proxy;
            }
            throw Unsupported.hit(spec.builder.getSimpleName() + "#" + name);
        }
    }
}
