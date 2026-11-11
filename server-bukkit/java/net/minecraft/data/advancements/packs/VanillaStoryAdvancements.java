// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.advancements.packs;
import net.minecraft.advancements.Advancement;
import net.minecraft.data.advancements.AdvancementSubProvider;
import net.minecraft.data.worldgen.BootstrapContext;
public class VanillaStoryAdvancements extends AdvancementSubProvider {
    public VanillaStoryAdvancements(BootstrapContext<Advancement> output) {
        super((net.minecraft.data.worldgen.BootstrapContext) null);
    }
    public void generate() {
    }
}
