// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.HashMap;
import java.util.Map;

public class ClassTreeIdRegistry {
    public static final int NO_ID_VALUE = -1;
    private final Map<Class<?>, Integer> lastIds = new HashMap<>();

    public int getLastIdFor(Class<?> clazz) {
        Integer own = lastIds.get(clazz);
        if (own != null) {
            return own;
        }
        for (Class<?> parent = clazz.getSuperclass(); parent != null && parent != Object.class; parent = parent.getSuperclass()) {
            Integer inherited = lastIds.get(parent);
            if (inherited != null) {
                return inherited;
            }
        }
        return NO_ID_VALUE;
    }

    public int getCount(Class<?> clazz) {
        return getLastIdFor(clazz) + 1;
    }

    public int define(Class<?> clazz) {
        int last = getLastIdFor(clazz);
        int next = last == NO_ID_VALUE ? 0 : last + 1;
        lastIds.put(clazz, next);
        return next;
    }
}
