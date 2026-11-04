// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.waypoints;

import com.mojang.datafixers.util.Either;
import io.netty.buffer.ByteBuf;
import java.util.UUID;
import net.minecraft.core.Vec3i;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

public abstract class TrackedWaypoint implements Waypoint {
    public static final StreamCodec<ByteBuf, TrackedWaypoint> STREAM_CODEC = StreamCodec.of((buf, waypoint) -> waypoint.write(buf), TrackedWaypoint::read);
    protected final Either<UUID, String> identifier;
    private final Waypoint.Icon icon;
    private final Type type;

    private TrackedWaypoint(Either<UUID, String> identifier, Waypoint.Icon icon, Type type) {
        this.identifier = identifier;
        this.icon = icon;
        this.type = type;
    }

    public Either<UUID, String> id() {
        return identifier;
    }

    public abstract void update(TrackedWaypoint other);

    public void write(ByteBuf buf) {
        FriendlyByteBuf out = new FriendlyByteBuf(buf);
        out.writeEither(identifier, net.minecraft.core.UUIDUtil.STREAM_CODEC, FriendlyByteBuf::writeUtf);
        Waypoint.Icon.STREAM_CODEC.encode(out, icon);
        out.writeEnum(type);
        writeContents(buf);
    }

    public abstract void writeContents(ByteBuf buf);

    private static TrackedWaypoint read(ByteBuf buf) {
        FriendlyByteBuf in = new FriendlyByteBuf(buf);
        Either<UUID, String> identifier = in.readEither(net.minecraft.core.UUIDUtil.STREAM_CODEC, FriendlyByteBuf::readUtf);
        Waypoint.Icon icon = Waypoint.Icon.STREAM_CODEC.decode(in);
        return switch (in.readEnum(Type.class)) {
            case EMPTY -> new EmptyWaypoint(identifier, icon, in);
            case VEC3I -> new Vec3iWaypoint(identifier, icon, in);
            case CHUNK -> new ChunkWaypoint(identifier, icon, in);
            case AZIMUTH -> new AzimuthWaypoint(identifier, icon, in);
        };
    }

    public static TrackedWaypoint setPosition(UUID identifier, Waypoint.Icon icon, Vec3i position) {
        return new Vec3iWaypoint(identifier, icon, position);
    }

    public static TrackedWaypoint setChunk(UUID identifier, Waypoint.Icon icon, ChunkPos chunk) {
        return new ChunkWaypoint(identifier, icon, chunk);
    }

    public static TrackedWaypoint setAzimuth(UUID identifier, Waypoint.Icon icon, float angle) {
        return new AzimuthWaypoint(identifier, icon, angle);
    }

    public static TrackedWaypoint empty(UUID identifier) {
        return new EmptyWaypoint(identifier);
    }

    public abstract double yawAngleToCamera(Level level, Camera camera, PartialTickSupplier partialTickSupplier);

    public abstract PitchDirection pitchDirectionToCamera(Level level, Projector projector, PartialTickSupplier partialTickSupplier);

    public abstract double distanceSquared(Entity fromEntity);

    public Waypoint.Icon icon() {
        return icon;
    }

    private static double yawTo(Camera camera, Vec3 target) {
        Vec3 away = camera.position().subtract(target);
        double x = -away.z;
        double z = away.x;
        return Mth.degreesDifference(camera.yaw(), (float) (Mth.atan2(z, x) * 57.2957763671875));
    }

    public static enum PitchDirection {
        NONE, UP, DOWN;
    }

    static enum Type {
        EMPTY, VEC3I, CHUNK, AZIMUTH;
    }

    static class EmptyWaypoint extends TrackedWaypoint {
        EmptyWaypoint(Either<UUID, String> identifier, Waypoint.Icon icon, FriendlyByteBuf byteBuf) {
            super(identifier, icon, Type.EMPTY);
        }

        EmptyWaypoint(UUID identifier) {
            super(Either.left(identifier), Waypoint.Icon.NULL, Type.EMPTY);
        }

        public void update(TrackedWaypoint other) {
        }

        public void writeContents(ByteBuf buf) {
        }

        public double yawAngleToCamera(Level level, Camera camera, PartialTickSupplier partialTickSupplier) {
            return Double.NaN;
        }

        public PitchDirection pitchDirectionToCamera(Level level, Projector projector, PartialTickSupplier partialTickSupplier) {
            return PitchDirection.NONE;
        }

        public double distanceSquared(Entity fromEntity) {
            return Double.POSITIVE_INFINITY;
        }
    }

    static class Vec3iWaypoint extends TrackedWaypoint {
        private Vec3i vector;

        public Vec3iWaypoint(UUID identifier, Waypoint.Icon icon, Vec3i vector) {
            super(Either.left(identifier), icon, Type.VEC3I);
            this.vector = vector;
        }

        public Vec3iWaypoint(Either<UUID, String> identifier, Waypoint.Icon icon, FriendlyByteBuf byteBuf) {
            super(identifier, icon, Type.VEC3I);
            this.vector = new Vec3i(byteBuf.readVarInt(), byteBuf.readVarInt(), byteBuf.readVarInt());
        }

        public void update(TrackedWaypoint other) {
            if (other instanceof Vec3iWaypoint moved) {
                vector = moved.vector;
            }
        }

        public void writeContents(ByteBuf buf) {
            FriendlyByteBuf out = new FriendlyByteBuf(buf);
            out.writeVarInt(vector.getX());
            out.writeVarInt(vector.getY());
            out.writeVarInt(vector.getZ());
        }

        private Vec3 position(Level level, PartialTickSupplier tick) {
            return identifier.left()
                .map(level::getEntity)
                .map(entity -> entity.blockPosition().distManhattan(vector) > 3 ? null : entity.getEyePosition(tick.apply(entity)))
                .orElseGet(() -> Vec3.atCenterOf(vector));
        }

        public double yawAngleToCamera(Level level, Camera camera, PartialTickSupplier partialTickSupplier) {
            return yawTo(camera, position(level, partialTickSupplier));
        }

        public PitchDirection pitchDirectionToCamera(Level level, Projector projector, PartialTickSupplier partialTickSupplier) {
            Vec3 screen = projector.projectPointToScreen(position(level, partialTickSupplier));
            boolean behind = screen.z > 1.0;
            double y = behind ? -screen.y : screen.y;
            if (y < -1.0) {
                return PitchDirection.DOWN;
            }
            if (y > 1.0) {
                return PitchDirection.UP;
            }
            if (behind && screen.y > 0.0) {
                return PitchDirection.UP;
            }
            if (behind && screen.y < 0.0) {
                return PitchDirection.DOWN;
            }
            return PitchDirection.NONE;
        }

        public double distanceSquared(Entity fromEntity) {
            return fromEntity.distanceToSqr(Vec3.atCenterOf(vector));
        }
    }

    static class ChunkWaypoint extends TrackedWaypoint {
        private ChunkPos chunkPos;

        public ChunkWaypoint(UUID identifier, Waypoint.Icon icon, ChunkPos chunkPos) {
            super(Either.left(identifier), icon, Type.CHUNK);
            this.chunkPos = chunkPos;
        }

        public ChunkWaypoint(Either<UUID, String> identifier, Waypoint.Icon icon, FriendlyByteBuf byteBuf) {
            super(identifier, icon, Type.CHUNK);
            this.chunkPos = new ChunkPos(byteBuf.readVarInt(), byteBuf.readVarInt());
        }

        public void update(TrackedWaypoint other) {
            if (other instanceof ChunkWaypoint moved) {
                chunkPos = moved.chunkPos;
            }
        }

        public void writeContents(ByteBuf buf) {
            FriendlyByteBuf out = new FriendlyByteBuf(buf);
            out.writeVarInt(chunkPos.x());
            out.writeVarInt(chunkPos.z());
        }

        public double yawAngleToCamera(Level level, Camera camera, PartialTickSupplier partialTickSupplier) {
            return yawTo(camera, Vec3.atCenterOf(chunkPos.getMiddleBlockPosition((int) camera.position().y())));
        }

        public PitchDirection pitchDirectionToCamera(Level level, Projector projector, PartialTickSupplier partialTickSupplier) {
            double horizon = projector.projectHorizonToScreen();
            return horizon < -1.0 ? PitchDirection.DOWN : horizon > 1.0 ? PitchDirection.UP : PitchDirection.NONE;
        }

        public double distanceSquared(Entity fromEntity) {
            return fromEntity.distanceToSqr(Vec3.atCenterOf(chunkPos.getMiddleBlockPosition(fromEntity.getBlockY())));
        }
    }

    static class AzimuthWaypoint extends TrackedWaypoint {
        private float angle;

        public AzimuthWaypoint(UUID identifier, Waypoint.Icon icon, float angle) {
            super(Either.left(identifier), icon, Type.AZIMUTH);
            this.angle = angle;
        }

        public AzimuthWaypoint(Either<UUID, String> identifier, Waypoint.Icon icon, FriendlyByteBuf byteBuf) {
            super(identifier, icon, Type.AZIMUTH);
            this.angle = byteBuf.readFloat();
        }

        public void update(TrackedWaypoint other) {
            if (other instanceof AzimuthWaypoint moved) {
                angle = moved.angle;
            }
        }

        public void writeContents(ByteBuf buf) {
            buf.writeFloat(angle);
        }

        public double yawAngleToCamera(Level level, Camera camera, PartialTickSupplier partialTickSupplier) {
            return Mth.degreesDifference(camera.yaw(), (float) (angle * 57.2957763671875));
        }

        public PitchDirection pitchDirectionToCamera(Level level, Projector projector, PartialTickSupplier partialTickSupplier) {
            double horizon = projector.projectHorizonToScreen();
            return horizon < -1.0 ? PitchDirection.DOWN : horizon > 1.0 ? PitchDirection.UP : PitchDirection.NONE;
        }

        public double distanceSquared(Entity fromEntity) {
            return Double.POSITIVE_INFINITY;
        }
    }

    public interface Camera {
        float yaw();

        Vec3 position();
    }

    public interface Projector {
        Vec3 projectPointToScreen(Vec3 point);

        double projectHorizonToScreen();
    }
}
