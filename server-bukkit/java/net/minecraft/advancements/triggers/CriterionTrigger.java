// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.Codec;
import net.minecraft.advancements.CriterionTriggerInstance;
public interface CriterionTrigger<T extends CriterionTriggerInstance> {
    Codec<T> codec();
    default Criterion<T> createCriterion(T instance) {
        return new Criterion<>(this, instance);
    }
}
