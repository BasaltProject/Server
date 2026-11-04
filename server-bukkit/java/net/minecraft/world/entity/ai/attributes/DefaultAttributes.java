// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import org.basaltmc.bukkit.LeftoverNatives;

public class DefaultAttributes {
    private static final AttributeSupplier kNone = AttributeSupplier.builder().build();
    private static final Map<Identifier, AttributeSupplier> kCache = new ConcurrentHashMap<>();

    private static AttributeSupplier build(Identifier key) {
        AttributeSupplier.Builder builder = AttributeSupplier.builder();
        boolean any = false;
        for (String entry : LeftoverNatives.entityDefaults(key.toString())) {
            int split = entry.lastIndexOf('=');
            Holder<Attribute> attribute = Attributes.registry().get(Identifier.parse(entry.substring(0, split))).orElse(null);
            if (attribute != null) {
                builder.add(attribute, Double.parseDouble(entry.substring(split + 1)));
                any = true;
            }
        }
        return any ? builder.build() : kNone;
    }

    private static AttributeSupplier of(EntityType<?> type) {
        return kCache.computeIfAbsent(EntityType.getKey(type), DefaultAttributes::build);
    }

    public static AttributeSupplier getSupplier(EntityType<? extends LivingEntity> type) {
        return of(type);
    }

    public static boolean hasSupplier(EntityType<?> type) {
        return of(type) != kNone;
    }

    public static void validate() {
    }
}
