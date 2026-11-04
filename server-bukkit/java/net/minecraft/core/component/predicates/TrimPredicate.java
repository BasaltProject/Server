// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.equipment.trim.ArmorTrim;
import net.minecraft.world.item.equipment.trim.TrimMaterial;
import net.minecraft.world.item.equipment.trim.TrimPattern;
import org.basaltmc.bukkit.facade.HolderSets;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record TrimPredicate(Optional<HolderSet<TrimMaterial>> material, Optional<HolderSet<TrimPattern>> pattern) implements SingleComponentItemPredicate<ArmorTrim> {
    public static final Codec<TrimPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        ReflectiveCodecs.holderSet(TrimMaterial.class).optionalFieldOf("material").forGetter(TrimPredicate::material),
        ReflectiveCodecs.holderSet(TrimPattern.class).optionalFieldOf("pattern").forGetter(TrimPredicate::pattern)).apply(i, TrimPredicate::new));
    public DataComponentType<ArmorTrim> componentType() {
        return DataComponents.TRIM;
    }
    public boolean matches(ArmorTrim value) {
        if (material.isPresent() && !HolderSets.contains(material.get(), value.material())) {
            return false;
        }
        return pattern.isEmpty() || HolderSets.contains(pattern.get(), value.pattern());
    }
}
