// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.advancements;
import java.util.List;
import net.minecraft.advancements.Advancement;
import net.minecraft.core.registries.SingleRegistryBootstrap;
import net.minecraft.data.worldgen.BootstrapContext;
public class AdvancementProvider implements SingleRegistryBootstrap<Advancement> {
    public AdvancementProvider(List<AdvancementSubProvider.Factory> subProviders) {
        
    }
    public void run(BootstrapContext<Advancement> output) {
    }
}
