package org.basaltmc.bukkit.facade;

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.core.MappedRegistry;
import net.minecraft.resources.Identifier;
import net.minecraft.tags.TagKey;
import org.basaltmc.bukkit.WorldNatives;

public final class RegistryTags {
    private RegistryTags() {}

    public static <T> void bind(MappedRegistry<T> registry, String nativeName) {
        String[] flat = WorldNatives.tags(nativeName);
        for (int i = 0; i + 1 < flat.length; i += 2) {
            List<Holder<T>> members = new ArrayList<>();
            if (!flat[i + 1].isEmpty()) {
                for (String member : flat[i + 1].split("\\|")) {
                    Identifier id = Identifier.tryParse(member);
                    if (id != null) {
                        registry.get(id).ifPresent(members::add);
                    }
                }
            }
            registry.bindTag(TagKey.create(registry.key(), Identifier.parse(flat[i])), members);
        }
    }
}
