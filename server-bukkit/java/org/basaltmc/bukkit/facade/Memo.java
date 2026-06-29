package org.basaltmc.bukkit.facade;

import java.util.Map;
import java.util.function.Function;

public final class Memo {
    private Memo() {}

    public static <K, V> V get(Map<K, V> map, K key, Function<? super K, ? extends V> maker) {
        V found = map.get(key);
        if (found != null) {
            return found;
        }
        V made = maker.apply(key);
        V raced = map.putIfAbsent(key, made);
        return raced != null ? raced : made;
    }
}
