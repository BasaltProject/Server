// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.commands.data;
import net.minecraft.world.entity.Entity;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.server.commands.ArgProvider;
import net.minecraft.world.entity.Entity;
public class EntityDataAccessor implements DataAccessor {
    public static final ArgProvider.Factory<DataAccessor> PROVIDER = arg -> ArgProvider.create("entity",
        () -> net.minecraft.commands.Commands.argument(arg, net.minecraft.commands.arguments.EntityArgument.entity()),
        context -> new EntityDataAccessor(net.minecraft.commands.arguments.EntityArgument.getEntity(context, arg)));
    private static final com.mojang.brigadier.exceptions.SimpleCommandExceptionType ERROR_NO_PLAYERS =
        new com.mojang.brigadier.exceptions.SimpleCommandExceptionType(Component.translatable("commands.data.entity.invalid"));
    private final Entity entity;
    public EntityDataAccessor(Entity entity) {
        this.entity = entity;
        
    }
    public void setData(CompoundTag tag) throws CommandSyntaxException {
        if (entity instanceof net.minecraft.world.entity.player.Player) {
            throw ERROR_NO_PLAYERS.create();
        }
        java.util.UUID uuid = entity.getUUID();
        entity.load(net.minecraft.world.level.storage.TagValueInput.create(net.minecraft.util.ProblemReporter.DISCARDING, entity.registryAccess(), tag));
        entity.setUUID(uuid);
        entity.postDataManipulated();
    }
    public CompoundTag getData() {
        return net.minecraft.advancements.predicates.NbtPredicate.getEntityTagToCompare(entity);
    }
    public Component getModifiedSuccess() {
        return Component.translatable("commands.data.entity.modified", entity.getDisplayName());
    }
    public Component getPrintSuccess(Tag data) {
        return Component.translatable("commands.data.entity.query", entity.getDisplayName(), net.minecraft.nbt.NbtUtils.toPrettyComponent(data));
    }
    public Component getPrintSuccess(NbtPathArgument.NbtPath path, double scale, int value) {
        return Component.translatable("commands.data.entity.get", path.asString(), entity.getDisplayName(), String.format(java.util.Locale.ROOT, "%.2f", scale), value);
    }
}
