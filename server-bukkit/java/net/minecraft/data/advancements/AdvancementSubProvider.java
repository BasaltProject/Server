// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.advancements;
import net.minecraft.advancements.Advancement;
import net.minecraft.advancements.predicates.DamageSourcePredicate;
import net.minecraft.core.HolderGetter;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.world.damagesource.DamageType;
public abstract class AdvancementSubProvider {
    protected final BootstrapContext<Advancement> output = null;
    protected final HolderGetter<DamageType> damageTypes = null;
    protected AdvancementSubProvider(BootstrapContext<Advancement> output) {
        
    }
    public abstract void generate();
    protected DamageSourcePredicate.Builder isProjectile() {
        return null;
    }
    public interface Factory {
        AdvancementSubProvider create(BootstrapContext<Advancement> output);
    }
}
