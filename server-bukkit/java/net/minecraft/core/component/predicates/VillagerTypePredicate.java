// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.entity.npc.villager.VillagerType;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record VillagerTypePredicate(HolderSet<VillagerType> villagerTypes) implements SingleComponentItemPredicate<Holder<VillagerType>> {
    public static final Codec<VillagerTypePredicate> CODEC = ReflectiveCodecs.holderSet(VillagerType.class).xmap(VillagerTypePredicate::new, VillagerTypePredicate::villagerTypes);
    public DataComponentType<Holder<VillagerType>> componentType() {
        return DataComponents.VILLAGER_VARIANT;
    }
    public boolean matches(Holder<VillagerType> villagerType) {
        return HolderSets.contains(villagerTypes, villagerType);
    }
    public static VillagerTypePredicate villagerTypes(HolderSet<VillagerType> villagerTypes) {
        return new VillagerTypePredicate(villagerTypes);
    }
}
