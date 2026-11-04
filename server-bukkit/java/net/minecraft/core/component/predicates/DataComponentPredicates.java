// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;

import java.util.IdentityHashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;

public class DataComponentPredicates {
    private static final Map<Identifier, DataComponentPredicate.Type<?>> kById = new LinkedHashMap<>();
    private static final Map<DataComponentPredicate.Type<?>, Identifier> kIds = new IdentityHashMap<>();

    public static final DataComponentPredicate.Type<DamagePredicate> DAMAGE = register("damage", DamagePredicate.CODEC);
    public static final DataComponentPredicate.Type<EnchantmentsPredicate.Enchantments> ENCHANTMENTS = register("enchantments", EnchantmentsPredicate.Enchantments.CODEC);
    public static final DataComponentPredicate.Type<EnchantmentsPredicate.StoredEnchantments> STORED_ENCHANTMENTS = register("stored_enchantments", EnchantmentsPredicate.StoredEnchantments.CODEC);
    public static final DataComponentPredicate.Type<PotionsPredicate> POTIONS = register("potion_contents", PotionsPredicate.CODEC);
    public static final DataComponentPredicate.Type<CustomDataPredicate> CUSTOM_DATA = register("custom_data", CustomDataPredicate.CODEC);
    public static final DataComponentPredicate.Type<ContainerPredicate> CONTAINER = register("container", ContainerPredicate.CODEC);
    public static final DataComponentPredicate.Type<BundlePredicate> BUNDLE_CONTENTS = register("bundle_contents", BundlePredicate.CODEC);
    public static final DataComponentPredicate.Type<FireworkExplosionPredicate> FIREWORK_EXPLOSION = register("firework_explosion", FireworkExplosionPredicate.CODEC);
    public static final DataComponentPredicate.Type<FireworksPredicate> FIREWORKS = register("fireworks", FireworksPredicate.CODEC);
    public static final DataComponentPredicate.Type<WritableBookPredicate> WRITABLE_BOOK = register("writable_book_content", WritableBookPredicate.CODEC);
    public static final DataComponentPredicate.Type<WrittenBookPredicate> WRITTEN_BOOK = register("written_book_content", WrittenBookPredicate.CODEC);
    public static final DataComponentPredicate.Type<AttributeModifiersPredicate> ATTRIBUTE_MODIFIERS = register("attribute_modifiers", AttributeModifiersPredicate.CODEC);
    public static final DataComponentPredicate.Type<TrimPredicate> ARMOR_TRIM = register("trim", TrimPredicate.CODEC);
    public static final DataComponentPredicate.Type<JukeboxPlayablePredicate> JUKEBOX_PLAYABLE = register("jukebox_playable", JukeboxPlayablePredicate.CODEC);
    public static final DataComponentPredicate.Type<VillagerTypePredicate> VILLAGER_VARIANT = register("villager/variant", VillagerTypePredicate.CODEC);

    private static <T extends DataComponentPredicate> DataComponentPredicate.Type<T> register(String name, com.mojang.serialization.Codec<T> codec) {
        DataComponentPredicate.Type<T> type = new DataComponentPredicate.ConcreteType<>(codec);
        Identifier id = Identifier.withDefaultNamespace(name);
        kById.put(id, type);
        kIds.put(type, id);
        return type;
    }

    static synchronized DataComponentPredicate.Type<?> byId(Identifier id) {
        DataComponentPredicate.Type<?> found = kById.get(id);
        if (found == null) {
            net.minecraft.core.component.DataComponentType<?> component = org.basaltmc.bukkit.facade.ComponentTypes.byId(id);
            if (component != null) {
                found = DataComponentPredicate.AnyValueType.create(component);
                kById.put(id, found);
                kIds.put(found, id);
            }
        }
        return found;
    }

    static synchronized Identifier idOf(DataComponentPredicate.Type<?> type) {
        return kIds.get(type);
    }

    public static DataComponentPredicate.Type<?> bootstrap(Registry<DataComponentPredicate.Type<?>> registry) {
        return DAMAGE;
    }
}
