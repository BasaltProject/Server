// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import com.mojang.brigadier.context.CommandContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.level.levelgen.Heightmap;
public class HeightmapTypeArgument extends StringRepresentableArgument<Heightmap.Types> {
    private static Heightmap.Types[] keptTypes() {
        return java.util.Arrays.stream(Heightmap.Types.values()).filter(Heightmap.Types::keepAfterWorldgen).toArray(Heightmap.Types[]::new);
    }

    private HeightmapTypeArgument() {
        super(null, HeightmapTypeArgument::keptTypes);
    }
    public static HeightmapTypeArgument heightmap() {
        return new HeightmapTypeArgument();
    }
    public static Heightmap.Types getHeightmap(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Heightmap.Types.class);
    }
    protected String convertId(String id) {
        return id.toLowerCase(java.util.Locale.ROOT);
    }
}
