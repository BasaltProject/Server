// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands.data;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.builder.ArgumentBuilder;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import java.util.List;
import net.minecraft.commands.CommandBuildContext;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.Tag;
import net.minecraft.server.commands.ArgProvider;
public class DataCommands {
    public static final List<ArgProvider.Factory<DataAccessor>> ALL_PROVIDERS = java.util.List.of(EntityDataAccessor.PROVIDER, BlockDataAccessor.PROVIDER, StorageDataAccessor.PROVIDER);
    public static final List<ArgProvider<DataAccessor>> TARGET_PROVIDERS = ArgProvider.buildList("target", ALL_PROVIDERS);
    public static final List<ArgProvider<DataAccessor>> SOURCE_PROVIDERS = ArgProvider.buildList("source", ALL_PROVIDERS);
    public static void register(CommandDispatcher<CommandSourceStack> dispatcher, CommandBuildContext buildContext) {
        org.basaltmc.bukkit.facade.NativeCommands.delegate(dispatcher, "data");
    }
    public static Tag getSingleTag(NbtPathArgument.NbtPath path, DataAccessor accessor) throws CommandSyntaxException {
        java.util.Collection<Tag> tags = path.get(accessor.getData());
        java.util.Iterator<Tag> iterator = tags.iterator();
        Tag first = iterator.next();
        if (iterator.hasNext()) {
            throw new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("commands.data.get.multiple")).create();
        }
        return first;
    }
    interface DataManipulator {
        int modify(CommandContext<CommandSourceStack> context, CompoundTag targetData, NbtPathArgument.NbtPath targetPath, List<Tag> source) throws CommandSyntaxException;
    }
    interface DataManipulatorDecorator {
        ArgumentBuilder<CommandSourceStack, ?> create(DataManipulator manipulator);
    }
    interface StringProcessor {
        String process(String string) throws CommandSyntaxException;
    }
}
