// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.advancements.packs;
import net.minecraft.advancements.Advancement;
import net.minecraft.core.registries.SingleRegistryBootstrap;
public class VanillaAdvancementProvider {
    public static SingleRegistryBootstrap<Advancement> create() {
        return context -> {
        };
    }
}
