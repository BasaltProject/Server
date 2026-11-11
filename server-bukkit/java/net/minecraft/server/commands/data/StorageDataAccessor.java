// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands.data;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.server.commands.ArgProvider;
import net.minecraft.world.level.storage.CommandStorage;
public class StorageDataAccessor implements DataAccessor {
    public static final ArgProvider.Factory<DataAccessor> PROVIDER = arg -> ArgProvider.create("storage",
        () -> net.minecraft.commands.Commands.argument(arg, net.minecraft.commands.arguments.IdentifierArgument.id())
            .suggests((context, builder) -> net.minecraft.commands.SharedSuggestionProvider.suggestResource(storageOf(context).keys(), builder)),
        context -> new StorageDataAccessor(storageOf(context), net.minecraft.commands.arguments.IdentifierArgument.getId(context, arg)));
    private final CommandStorage storage;
    private final Identifier id;

    private static CommandStorage storageOf(com.mojang.brigadier.context.CommandContext<net.minecraft.commands.CommandSourceStack> context) {
        return context.getSource().getServer().getCommandStorage();
    }
    private StorageDataAccessor(CommandStorage storage, Identifier id) {
        this.storage = storage;
        this.id = id;
        
    }
    public void setData(CompoundTag tag) {
        storage.set(id, tag);
    }
    public CompoundTag getData() {
        return storage.get(id);
    }
    public Component getModifiedSuccess() {
        return Component.translatable("commands.data.storage.modified", Component.translationArg(id));
    }
    public Component getPrintSuccess(Tag data) {
        return Component.translatable("commands.data.storage.query", Component.translationArg(id), net.minecraft.nbt.NbtUtils.toPrettyComponent(data));
    }
    public Component getPrintSuccess(NbtPathArgument.NbtPath path, double scale, int value) {
        return Component.translatable("commands.data.storage.get", path.asString(), Component.translationArg(id), String.format(java.util.Locale.ROOT, "%.2f", scale), value);
    }
}
