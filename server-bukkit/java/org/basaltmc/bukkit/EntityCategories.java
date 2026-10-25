package org.basaltmc.bukkit;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

final class EntityCategories {
    private static volatile Map<String, String> categories;

    private EntityCategories() {}

    static String of(String type) {
        if (categories == null) {
            synchronized (EntityCategories.class) {
                if (categories == null) {
                    Map<String, String> read = new HashMap<>();
                    try (InputStream in = EntityCategories.class.getResourceAsStream("/entity_categories.txt")) {
                        if (in != null) {
                            for (String line : new String(in.readAllBytes(), StandardCharsets.UTF_8).split("\n")) {
                                String[] pair = line.strip().split(" ");
                                if (pair.length == 2) {
                                    read.put(pair[0], pair[1]);
                                }
                            }
                        }
                    } catch (IOException failure) {
                        Natives.log(2, "Bukkit", "Could not read the entity categories: " + failure);
                    }
                    categories = read;
                }
            }
        }
        return categories.getOrDefault(type, "misc");
    }
}
