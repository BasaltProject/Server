// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;
import net.minecraft.util.ProblemReporter;
import net.minecraft.world.level.storage.TagValueOutput;
import com.mojang.datafixers.DataFixer;
import java.util.Optional;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.server.players.NameAndId;
import net.minecraft.world.entity.player.Player;
public class PlayerDataStorage {
    private final java.util.Map<java.util.UUID, CompoundTag> held = new java.util.concurrent.ConcurrentHashMap<>();
    protected final DataFixer fixerUpper;
    public PlayerDataStorage(LevelStorageSource.LevelStorageAccess levelAccess, DataFixer fixerUpper) {
        this.fixerUpper = fixerUpper;
        
    }
    public void save(Player player) {
        TagValueOutput output = TagValueOutput.createWithContext(ProblemReporter.DISCARDING, player.registryAccess());
        player.saveWithoutId(output);
        held.put(player.getUUID(), output.buildResult());
    }
    public Optional<CompoundTag> load(NameAndId nameAndId) {
        return Optional.ofNullable(held.get(nameAndId.id()));


    }
}
