// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.feature.Feature;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public class PlaceCommand {
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext context) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "place");
    }
    private static String at(BlockPos pos) {
        return pos.getX() + " " + pos.getY() + " " + pos.getZ();
    }

    public static int placeFeature(CommandSourceStack source, Holder<Feature> featureHolder, BlockPos pos) throws CommandSyntaxException {
        return org.basaltmc.bukkit.facade.NativeCommands.run(source, "place feature " + featureHolder.getRegisteredName() + " " + at(pos));
    }
    public static int placeJigsaw(CommandSourceStack source, Holder<StructureTemplatePool> pool, Identifier target, int maxDepth, BlockPos pos) throws CommandSyntaxException {
        return org.basaltmc.bukkit.facade.NativeCommands.run(source, "place jigsaw " + pool.getRegisteredName() + " " + target + " " + maxDepth + " " + at(pos));
    }
    public static int placeStructure(CommandSourceStack source, Holder.Reference<Structure> structureHolder, BlockPos pos) throws CommandSyntaxException {
        return org.basaltmc.bukkit.facade.NativeCommands.run(source, "place structure " + structureHolder.key().identifier() + " " + at(pos));
    }
    public static int placeTemplate(CommandSourceStack source, Identifier template, BlockPos pos, Rotation rotation, Mirror mirror, float integrity, int seed, boolean strict) throws CommandSyntaxException {
        return org.basaltmc.bukkit.facade.NativeCommands.run(source, "place template " + template + " " + at(pos) + " " + rotation.getSerializedName() + " " + mirror.getSerializedName() + " " + integrity + " " + seed);
    }
}
