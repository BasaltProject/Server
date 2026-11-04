// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.advancements.predicates.CollectionPredicate;
import net.minecraft.advancements.predicates.MobEffectsPredicate;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.item.alchemy.Potion;
import net.minecraft.world.item.alchemy.PotionContents;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record PotionsPredicate(Optional<HolderSet<Potion>> potions, Optional<CollectionPredicate<MobEffectInstance, MobEffectsPredicate>> effects) implements SingleComponentItemPredicate<PotionContents> {
    public static final Codec<PotionsPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        ReflectiveCodecs.holderSet(Potion.class).optionalFieldOf("potions").forGetter(PotionsPredicate::potions),
        CollectionPredicate.<MobEffectInstance, MobEffectsPredicate>codec(MobEffectsPredicate.CODEC).optionalFieldOf("effects").forGetter(PotionsPredicate::effects)).apply(i, PotionsPredicate::new));
    public DataComponentType<PotionContents> componentType() {
        return DataComponents.POTION_CONTENTS;
    }
    public boolean matches(PotionContents contents) {
        Optional<Holder<Potion>> potion = contents.potion();
        if (potions.isPresent() && (potion.isEmpty() || !HolderSets.contains(potions.get(), potion.get()))) {
            return false;
        }
        return effects.isEmpty() || effects.get().test(contents.getAllEffects());
    }
    public static PotionsPredicate ofPotions(HolderSet<Potion> potions) {
        return new PotionsPredicate(Optional.of(potions), Optional.empty());
    }
    public static PotionsPredicate ofPotion(Holder<Potion> potion) {
        return ofPotions(HolderSet.direct(potion));
    }
}
