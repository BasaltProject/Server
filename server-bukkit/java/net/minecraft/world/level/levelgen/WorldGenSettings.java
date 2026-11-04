// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.saveddata.SavedData;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.serialization.Codec;
import net.minecraft.core.RegistryAccess;
import net.minecraft.world.level.saveddata.SavedDataType;
public class WorldGenSettings extends SavedData {
    public WorldGenSettings(WorldOptions worldOptions, WorldDimensions worldDimensions) {
    }
    public WorldDimensions dimensions() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.WorldGenSettings.dimensions");
    }
    public WorldOptions options() {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.WorldGenSettings.options");
    }
    public static final Codec<WorldGenSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.WorldGenSettings.class, "CODEC");
    public static final SavedDataType<WorldGenSettings> TYPE = null;
    public static WorldGenSettings of(WorldOptions options, RegistryAccess registryAccess) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.WorldGenSettings#of");
    }
    public int hashCode() {
        return super.hashCode();
    }
    public String toString() {
        return super.toString();
    }
}
