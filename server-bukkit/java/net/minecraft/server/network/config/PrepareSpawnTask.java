// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network.config;

import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.registries.Registries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.network.Connection;
import net.minecraft.network.protocol.Packet;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.server.level.TicketType;
import net.minecraft.server.network.CommonListenerCookie;
import net.minecraft.server.network.ConfigurationTask;
import net.minecraft.server.players.NameAndId;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.LevelData;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.Vec3;

public class PrepareSpawnTask implements ConfigurationTask {
    public static final ConfigurationTask.Type TYPE = new ConfigurationTask.Type("prepare_spawn");
    public static final int PREPARE_CHUNK_RADIUS = 3;
    private final MinecraftServer server;
    private final NameAndId nameAndId;
    private State state;

    public PrepareSpawnTask(MinecraftServer server, NameAndId nameAndId) {
        this.server = server;
        this.nameAndId = nameAndId;
    }

    public void start(Consumer<Packet<?>> connection) {
        CompoundTag saved = server.getPlayerList().loadPlayerData(nameAndId).orElse(new CompoundTag());
        LevelData.RespawnData respawn = server.getWorldData().overworldData().getRespawnData();
        ServerLevel level = savedLevel(saved);
        if (level == null) {
            level = server.getLevel(respawn.globalPos().dimension());
        }
        if (level == null) {
            level = server.overworld();
        }
        ListTag pos = saved.getListOrEmpty("Pos");
        Vec3 where = pos.size() == 3 ? new Vec3(pos.getDoubleOr(0, 0), pos.getDoubleOr(1, 0), pos.getDoubleOr(2, 0))
            : Vec3.atBottomCenterOf(respawn.globalPos().pos());
        ListTag rotation = saved.getListOrEmpty("Rotation");
        Vec2 angle = rotation.size() == 2 ? new Vec2(rotation.getFloatOr(0, 0.0F), rotation.getFloatOr(1, 0.0F)) : new Vec2(respawn.yaw(), respawn.pitch());
        state = new Preparing(level, where, angle);
    }

    private ServerLevel savedLevel(CompoundTag saved) {
        Optional<String> dimension = saved.getString("Dimension");
        if (dimension.isEmpty()) {
            return null;
        }
        Identifier id = Identifier.tryParse(dimension.get());
        return id == null ? null : server.getLevel(ResourceKey.create(Registries.DIMENSION, id));
    }

    public boolean tick() {
        if (state instanceof Preparing preparing) {
            Ready ready = preparing.tick();
            if (ready != null) {
                state = ready;
                return true;
            }
            return false;
        }
        return state instanceof Ready;
    }

    public ServerPlayer spawnPlayer(Connection connection, CommonListenerCookie cookie) {
        if (state instanceof Ready ready) {
            return ready.spawn(connection, cookie);
        }
        throw new IllegalStateException("Player spawn was not ready");
    }

    public void keepAlive() {
        if (state instanceof Ready ready) {
            ready.keepAlive();
        }
    }

    public void close() {
        if (state instanceof Preparing preparing) {
            preparing.cancel();
        }
        state = null;
    }

    public ConfigurationTask.Type type() {
        return TYPE;
    }

    private static ChunkPos chunkOf(Vec3 position) {
        return ChunkPos.containing(BlockPos.containing(position));
    }

    final class Preparing implements State {
        private final ServerLevel level;
        private final Vec3 position;
        private final Vec2 angle;
        private CompletableFuture<?> loading;

        private Preparing(ServerLevel spawnLevel, Vec3 spawnPosition, Vec2 spawnAngle) {
            this.level = spawnLevel;
            this.position = spawnPosition;
            this.angle = spawnAngle;
        }

        public void cancel() {
            if (loading != null) {
                loading.cancel(false);
            }
        }

        public Ready tick() {
            if (loading == null) {
                loading = level.getChunkSource().addTicketAndLoadWithRadius(TicketType.PLAYER_SPAWN, chunkOf(position), PREPARE_CHUNK_RADIUS);
            }
            return loading.isDone() ? new Ready(level, position, angle) : null;
        }
    }

    final class Ready implements State {
        private final ServerLevel level;
        private final Vec3 position;
        private final Vec2 angle;

        private Ready(ServerLevel spawnLevel, Vec3 spawnPosition, Vec2 spawnAngle) {
            this.level = spawnLevel;
            this.position = spawnPosition;
            this.angle = spawnAngle;
        }

        public void keepAlive() {
            level.getChunkSource().addTicketWithRadius(TicketType.PLAYER_SPAWN, chunkOf(position), PREPARE_CHUNK_RADIUS);
        }

        public ServerPlayer spawn(Connection connection, CommonListenerCookie cookie) {
            level.waitForEntities(chunkOf(position), PREPARE_CHUNK_RADIUS);
            ServerPlayer player = new ServerPlayer(server, level, cookie.gameProfile(), cookie.clientInformation());
            player.snapTo(position, angle.x, angle.y);
            server.getPlayerList().placeNewPlayer(connection, player, cookie);
            return player;
        }
    }

    interface State {}
}
