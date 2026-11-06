// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;

public class DataPackConfig {
    public static final DataPackConfig DEFAULT = new DataPackConfig(List.of("vanilla"), List.of());
    public static final Codec<DataPackConfig> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.STRING.listOf().fieldOf("Enabled").forGetter(DataPackConfig::getEnabled),
        Codec.STRING.listOf().fieldOf("Disabled").forGetter(DataPackConfig::getDisabled)).apply(i, DataPackConfig::new));
    private final List<String> enabled;
    private final List<String> disabled;

    public DataPackConfig(List<String> enabled, List<String> disabled) {
        this.enabled = List.copyOf(enabled);
        this.disabled = List.copyOf(disabled);
    }

    public List<String> getEnabled() {
        return enabled;
    }

    public List<String> getDisabled() {
        return disabled;
    }
}
