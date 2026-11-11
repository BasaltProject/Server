// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number;
import java.util.List;
import net.minecraft.nbt.NumericTag;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.CompoundTag;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.MapCodec;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.storage.loot.LootContext;
public record StoredNumberAccess(Identifier storage, NbtPathArgument.NbtPath path) {
    public static final MapCodec<StoredNumberAccess> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.providers.number.StoredNumberAccess.class, "MAP_CODEC");
    public Number getNumericTag(LootContext context) {
        CompoundTag stored = context.getLevel().getServer().getCommandStorage().get(storage);
        try {
            List<Tag> picked = path.get(stored);
            if (picked.size() == 1 && picked.getFirst() instanceof NumericTag number) {
                return number.box();
            }
        } catch (CommandSyntaxException ignored) {
        }
        return null;
    }
}
