// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;
import com.mojang.datafixers.DataFixer;
import com.mojang.serialization.Codec;
import java.nio.file.Path;
import java.util.Map;
import java.util.function.BiConsumer;
import net.minecraft.advancements.AdvancementHolder;
import net.minecraft.advancements.AdvancementProgress;
import net.minecraft.advancements.CriterionTriggerInstance;
import net.minecraft.advancements.triggers.CriterionTrigger;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.players.PlayerList;
public class PlayerAdvancements {
    private ServerPlayer player;
    public PlayerAdvancements(DataFixer dataFixer, PlayerList playerList, ServerAdvancementManager manager, Path playerSavePath, ServerPlayer player) {
        
    }
    public void setPlayer(ServerPlayer player) {
        this.player = player;
    }
    public void clearTriggers() {
    }
    public void reload(ServerAdvancementManager manager) {
    }
    public void save() {
    }
    public boolean award(AdvancementHolder holder, String criterion) {
        org.bukkit.entity.Player bukkit = player == null ? null : player.bukkitPlayer();
        org.bukkit.advancement.Advancement advancement = org.basaltmc.bukkit.misc.BasaltAdvancement.get(org.bukkit.NamespacedKey.fromString(holder.id().toString()));
        return bukkit != null && advancement != null && bukkit.getAdvancementProgress(advancement).awardCriteria(criterion);
    }
    public boolean revoke(AdvancementHolder advancement, String criterion) {
        org.bukkit.entity.Player bukkit = player == null ? null : player.bukkitPlayer();
        org.bukkit.advancement.Advancement found = org.basaltmc.bukkit.misc.BasaltAdvancement.get(org.bukkit.NamespacedKey.fromString(advancement.id().toString()));
        return bukkit != null && found != null && bukkit.getAdvancementProgress(found).revokeCriteria(criterion);
    }
    public void flushDirty(ServerPlayer player, boolean showAdvancements) {
    }
    public void setSelectedTab(AdvancementHolder holder) {
    }
    public AdvancementProgress getOrStartProgress(AdvancementHolder advancement) {
        org.bukkit.entity.Player bukkit = player == null ? null : player.bukkitPlayer();
        org.bukkit.advancement.Advancement found = org.basaltmc.bukkit.misc.BasaltAdvancement.get(org.bukkit.NamespacedKey.fromString(advancement.id().toString()));
        Map<String, java.time.Instant> obtained = new java.util.HashMap<>();
        if (bukkit != null && found != null) {
            org.bukkit.advancement.AdvancementProgress progress = bukkit.getAdvancementProgress(found);
            for (String name : progress.getAwardedCriteria()) {
                java.util.Date when = progress.getDateAwarded(name);
                obtained.put(name, when == null ? java.time.Instant.now() : when.toInstant());
            }
        }
        return AdvancementProgress.of(advancement.value().requirements(), obtained);
    }
    @SuppressWarnings("unchecked")
    public <T extends CriterionTriggerInstance> Map<TriggerInstanceKey, T> getTriggerMapForType(CriterionTrigger<T> type) {
        Map<TriggerInstanceKey, T> listeners = new java.util.LinkedHashMap<>();
        ServerAdvancementManager manager = player == null ? null : player.level().getServer().getAdvancements();
        if (manager == null) {
            return listeners;
        }
        for (AdvancementHolder holder : manager.getAllAdvancements()) {
            AdvancementProgress progress = getOrStartProgress(holder);
            holder.value().criteria().forEach((name, criterion) -> {
                net.minecraft.advancements.CriterionProgress state = progress.getCriterion(name);
                if (criterion.trigger() == type && state != null && !state.isDone()) {
                    listeners.put(new TriggerInstanceKey(holder, name), (T) criterion.triggerInstance());
                }
            });
        }
        return listeners;
    }
    public static record TriggerInstanceKey(AdvancementHolder advancement, String criterion) {
    }
    static record Data(Map<Identifier, AdvancementProgress> map) {
        public static final Codec<Data> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.PlayerAdvancements.Data.class, "CODEC");
        public void forEach(BiConsumer<Identifier, AdvancementProgress> consumer) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.PlayerAdvancements.Data#forEach");
        }
    }
}
