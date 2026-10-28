package compatibility;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.Statement;
import net.minecraft.resources.ResourceKey;

public final class RuntimeCompatibilityTest {
    private static native boolean hostEnvironment();
    public static void javaOwnedEnvironments() throws Exception {
        java.util.concurrent.atomic.AtomicReference<Throwable> failure = new java.util.concurrent.atomic.AtomicReference<>();
        for (int round = 0; round < 8; ++round) {
            Thread[] workers = new Thread[8];
            for (int i = 0; i < workers.length; ++i) {
                workers[i] = new Thread(() -> {
                    try {
                        for (int j = 0; j < 50; ++j)
                            if (!hostEnvironment()) throw new AssertionError("Lost Java-owned JNI environment");
                    } catch (Throwable e) { failure.compareAndSet(null, e); }
                });
                workers[i].start();
            }
            for (Thread worker : workers) worker.join();
            System.gc();
        }
        if (failure.get() != null) throw new AssertionError(failure.get());
    }

    public static final class OptionalIntegration {
        private MissingDependency optionalImplementation() { return null; }
        public static String available() { return "available"; }
    }

    public interface ReflectionRoot { default String echo() { return "ok"; } }
    public interface ReflectionD0 extends ReflectionRoot { }
    public interface ReflectionL1 extends ReflectionD0 { }
    public interface ReflectionR1 extends ReflectionD0 { }
    public interface ReflectionD1 extends ReflectionL1, ReflectionR1 { }
    public interface ReflectionL2 extends ReflectionD1 { }
    public interface ReflectionR2 extends ReflectionD1 { }
    public interface ReflectionD2 extends ReflectionL2, ReflectionR2 { }
    public interface ReflectionL3 extends ReflectionD2 { }
    public interface ReflectionR3 extends ReflectionD2 { }
    public interface ReflectionD3 extends ReflectionL3, ReflectionR3 { }
    public interface ReflectionL4 extends ReflectionD3 { }
    public interface ReflectionR4 extends ReflectionD3 { }
    public interface ReflectionD4 extends ReflectionL4, ReflectionR4 { }
    public interface ReflectionL5 extends ReflectionD4 { }
    public interface ReflectionR5 extends ReflectionD4 { }
    public interface ReflectionD5 extends ReflectionL5, ReflectionR5 { }
    public interface ReflectionL6 extends ReflectionD5 { }
    public interface ReflectionR6 extends ReflectionD5 { }
    public interface ReflectionD6 extends ReflectionL6, ReflectionR6 { }
    public interface ReflectionL7 extends ReflectionD6 { }
    public interface ReflectionR7 extends ReflectionD6 { }
    public interface ReflectionD7 extends ReflectionL7, ReflectionR7 { }
    public interface ReflectionL8 extends ReflectionD7 { }
    public interface ReflectionR8 extends ReflectionD7 { }
    public interface ReflectionD8 extends ReflectionL8, ReflectionR8 { }
    public interface ReflectionL9 extends ReflectionD8 { }
    public interface ReflectionR9 extends ReflectionD8 { }
    public interface ReflectionD9 extends ReflectionL9, ReflectionR9 { }
    public interface ReflectionL10 extends ReflectionD9 { }
    public interface ReflectionR10 extends ReflectionD9 { }
    public interface ReflectionD10 extends ReflectionL10, ReflectionR10 { }
    public interface ReflectionL11 extends ReflectionD10 { }
    public interface ReflectionR11 extends ReflectionD10 { }
    public interface ReflectionD11 extends ReflectionL11, ReflectionR11 { }
    public interface ReflectionL12 extends ReflectionD11 { }
    public interface ReflectionR12 extends ReflectionD11 { }
    public interface ReflectionD12 extends ReflectionL12, ReflectionR12 { }
    public static void reflectionDiamonds() throws Exception {
        if (ReflectionD12.class.getMethods().length != 1) throw new AssertionError("Repeated inherited methods");
        if (ReflectionD12.class.getMethod("echo").getDeclaringClass() != ReflectionRoot.class) throw new AssertionError("Wrong interface owner");
    }

    public static void hashedArrayClones() {
        for (int round = 0; round < 12; ++round) {
            Object marker = new Object();
            Object[] source = {marker, "retained"};
            int sourceHash = System.identityHashCode(source);
            int[] primitive = {round, 17, 31};
            int primitiveHash = System.identityHashCode(primitive);
            System.gc();
            Object[][] copies = new Object[32][];
            int[][] values = new int[32][];
            int[] hashes = new int[32];
            for (int i = 0; i < copies.length; ++i) {
                copies[i] = source.clone();
                values[i] = primitive.clone();
                hashes[i] = System.identityHashCode(copies[i]);
            }
            System.gc();
            if (sourceHash != System.identityHashCode(source) || primitiveHash != System.identityHashCode(primitive)) throw new AssertionError("Source identity changed");
            for (int i = 0; i < copies.length; ++i) {
                if (copies[i] == source || copies[i][0] != marker || !"retained".equals(copies[i][1])) throw new AssertionError("Reference clone corrupted");
                if (values[i] == primitive || values[i][0] != round || values[i][2] != 31) throw new AssertionError("Primitive clone corrupted");
                if (hashes[i] != System.identityHashCode(copies[i])) throw new AssertionError("Clone identity changed");
            }
        }
    }

    static final class ReturnMarker { final int value; ReturnMarker(int value) { this.value = value; } }
    static final class ReturnSource {
        synchronized Object[] value(int n) { return new Object[] {new ReturnMarker(n), "live"}; }
    }
    public static void synchronizedReturns() throws Exception {
        ReturnSource source = new ReturnSource();
        java.util.concurrent.atomic.AtomicReference<Throwable> failure = new java.util.concurrent.atomic.AtomicReference<>();
        Thread[] workers = new Thread[4];
        for (int t = 0; t < workers.length; ++t) {
            workers[t] = new Thread(() -> {
                try {
                    for (int i = 0; i < 5000; ++i) {
                        Object[] value = source.value(i);
                        if (value.length != 2 || !(value[0] instanceof ReturnMarker) || ((ReturnMarker) value[0]).value != i || !"live".equals(value[1])) throw new AssertionError("Corrupt synchronized return");
                    }
                } catch (Throwable error) { failure.set(error); }
            });
            workers[t].start();
        }
        for (int i = 0; i < 40; ++i) { System.gc(); Thread.sleep(1); }
        for (Thread worker : workers) worker.join();
        if (failure.get() != null) throw new AssertionError(failure.get());
    }

    public static void publicReflection() throws Exception {
        Object value = OptionalIntegration.class.getMethod("available").invoke(null);
        if (!"available".equals(value)) throw new AssertionError("Public method was not invoked");
        for (java.lang.reflect.Method method : OptionalIntegration.class.getMethods()) {
            if (!Modifier.isPublic(method.getModifiers())) throw new AssertionError("Private method leaked");
        }
    }

    public static void featureKeys() throws Exception {
        for (String name : new String[] {"AquaticFeatures", "CaveFeatures", "EndFeatures", "NetherFeatures",
                "PileFeatures", "TreeFeatures", "VegetationFeatures"}) {
            Class<?> type = Class.forName("net.minecraft.data.worldgen.features." + name);
            int count = 0;
            for (Field field : type.getFields()) {
                if (field.getType() != ResourceKey.class) continue;
                ResourceKey<?> key = (ResourceKey<?>) field.get(null);
                if (key == null || !"minecraft:worldgen/configured_feature".equals(key.registry().toString())) {
                    throw new AssertionError("Invalid configured feature key: " + field);
                }
                count++;
            }
            if (count == 0) throw new AssertionError("No configured feature keys in " + name);
        }
    }

    public static void sqlite() throws Exception {
        // A consumer may inspect and close a JAR before SLF4J/JDBC are first
        // initialized. Later service discovery must reopen it, not reuse the
        // closed handle retained by the resource cache.
        java.net.URL service = RuntimeCompatibilityTest.class.getClassLoader()
                .getResource("META-INF/services/org.slf4j.spi.SLF4JServiceProvider");
        if (service == null || !(service.openConnection() instanceof java.net.JarURLConnection jarConnection)) {
            throw new AssertionError("Shared logging provider resource is missing");
        }
        jarConnection.getJarFile().close();
        Class.forName("org.sqlite.JDBC");
        try (Connection connection = DriverManager.getConnection("jdbc:sqlite::memory:");
                Statement statement = connection.createStatement()) {
            statement.executeUpdate("CREATE TABLE compatibility (value INTEGER NOT NULL)");
            statement.executeUpdate("INSERT INTO compatibility VALUES (42)");
            try (ResultSet result = statement.executeQuery("SELECT value FROM compatibility")) {
                if (!result.next() || result.getInt(1) != 42) throw new AssertionError("SQLite round trip failed");
            }
        }
    }

    public static void registryTags() throws Exception {
        // Tag's static constants ask Bukkit for tags during initialization.
        // This JNI fixture has no running server; only those lookups are stubbed.
        org.bukkit.Server server = (org.bukkit.Server) java.lang.reflect.Proxy.newProxyInstance(
                org.bukkit.Server.class.getClassLoader(), new Class<?>[] {org.bukkit.Server.class},
                (proxy, method, args) -> switch (method.getName()) {
                    case "getLogger" -> java.util.logging.Logger.getLogger("compatibility");
                    case "getName", "getVersion", "getBukkitVersion" -> "compatibility";
                    default -> null;
                });
        org.bukkit.Bukkit.setServer(server);
        Class<?> tags = Class.forName("org.basaltmc.bukkit.BasaltTag");
        var all = tags.getDeclaredMethod("all", String.class, Class.class);
        var invalidate = tags.getDeclaredMethod("invalidate");
        all.setAccessible(true);
        invalidate.setAccessible(true);
        for (int pass = 0; pass < 2; ++pass) {
            java.util.Map<?, ?> snapshot = (java.util.Map<?, ?>) all.invoke(null, org.bukkit.Tag.REGISTRY_BLOCKS, org.bukkit.Material.class);
            if (snapshot != all.invoke(null, org.bukkit.Tag.REGISTRY_BLOCKS, org.bukkit.Material.class)) {
                throw new AssertionError("Tag snapshot was not cached");
            }
            for (String name : new String[] {"first", "second"}) {
                @SuppressWarnings("unchecked")
                org.bukkit.Tag<org.bukkit.Material> tag = (org.bukkit.Tag<org.bukkit.Material>) snapshot.get(org.bukkit.NamespacedKey.minecraft(name));
                if (tag == null || !tag.getValues().equals(java.util.Set.of(org.bukkit.Material.STONE, org.bukkit.Material.DIRT))) {
                    throw new AssertionError("Shared tag members were not retained");
                }
            }
            invalidate.invoke(null);
        }
    }

    public static byte[][] scoreboardPackets() throws Exception {
        var fancy = net.kyori.adventure.text.Component.text("Title", net.kyori.adventure.text.format.NamedTextColor.GOLD)
            .decorate(net.kyori.adventure.text.format.TextDecoration.BOLD);
        var title = io.papermc.paper.adventure.PaperAdventure.asVanilla(fancy);
        if (!io.papermc.paper.adventure.PaperAdventure.asAdventure(title).equals(fancy))
            throw new AssertionError("Adventure styles lost");
        var board = new net.minecraft.world.scores.Scoreboard();
        var objective = board.addObjective("compat", net.minecraft.world.scores.criteria.ObjectiveCriteria.DUMMY,
            title, net.minecraft.world.scores.criteria.ObjectiveCriteria.RenderType.INTEGER, false,
            net.minecraft.network.chat.numbers.BlankFormat.INSTANCE);
        if (board.getObjective("compat") != objective) throw new AssertionError("Objective not retained");
        var team = new net.minecraft.world.scores.PlayerTeam(board, "team");
        team.setPlayerPrefix(net.minecraft.network.chat.Component.literal("prefix "));
        team.setPlayerSuffix(net.minecraft.network.chat.Component.literal(" suffix"));
        team.setColor(net.minecraft.ChatFormatting.RED);
        team.setNameTagVisibility(net.minecraft.world.scores.Team.Visibility.NEVER);
        team.setCollisionRule(net.minecraft.world.scores.Team.CollisionRule.PUSH_OTHER_TEAMS);
        team.setAllowFriendlyFire(false);
        team.getPlayers().add("entry");
        var teamAdd = net.minecraft.network.protocol.game.ClientboundSetPlayerTeamPacket.createAddOrModifyPacket(team, true);
        team.setPlayerPrefix(net.minecraft.network.chat.Component.literal("changed"));
        net.minecraft.network.protocol.Packet<?>[] packets = {
            new net.minecraft.network.protocol.game.ClientboundSetObjectivePacket(objective, 0),
            new net.minecraft.network.protocol.game.ClientboundSetDisplayObjectivePacket(net.minecraft.world.scores.DisplaySlot.SIDEBAR, objective),
            new net.minecraft.network.protocol.game.ClientboundSetScorePacket("entry", "compat", -7,
                java.util.Optional.of(title), java.util.Optional.of(new net.minecraft.network.chat.numbers.FixedFormat(title))),
            teamAdd,
            net.minecraft.network.protocol.game.ClientboundSetPlayerTeamPacket.createAddOrModifyPacket(team, false),
            net.minecraft.network.protocol.game.ClientboundSetPlayerTeamPacket.createPlayerPacket(team,"other",net.minecraft.network.protocol.game.ClientboundSetPlayerTeamPacket.Action.ADD),
            net.minecraft.network.protocol.game.ClientboundSetPlayerTeamPacket.createPlayerPacket(team,"other",net.minecraft.network.protocol.game.ClientboundSetPlayerTeamPacket.Action.REMOVE),
            net.minecraft.network.protocol.game.ClientboundSetPlayerTeamPacket.createRemovePacket(team),
            new net.minecraft.network.protocol.game.ClientboundResetScorePacket("entry", "compat"),
            new net.minecraft.network.protocol.game.ClientboundResetScorePacket("entry", null),
            new net.minecraft.network.protocol.game.ClientboundSetObjectivePacket(objective, 1),
            new net.minecraft.network.protocol.game.ClientboundSetDisplayObjectivePacket(net.minecraft.world.scores.DisplaySlot.SIDEBAR, null)
        };
        var channel = new io.netty.channel.embedded.EmbeddedChannel(new org.basaltmc.bukkit.net.WirePacketEncoder());
        var connection = new net.minecraft.server.network.ServerGamePacketListenerImpl(new net.minecraft.network.Connection(channel));
        byte[][] frames = new byte[packets.length][];
        try {
            for (int i = 0; i < packets.length; ++i) {
                connection.send(packets[i]);
                io.netty.buffer.ByteBuf frame = channel.readOutbound();
                if (frame == null) throw new AssertionError("Packet was not sent");
                try { frames[i] = new byte[frame.readableBytes()]; frame.readBytes(frames[i]); }
                finally { frame.release(); }
            }
        } finally { channel.finishAndReleaseAll(); }
        return frames;
    }

    public static byte[] metadataPacket() {
        var entity = new net.minecraft.world.entity.Entity() {};
        var flags = net.minecraft.world.entity.Entity.DATA_SHARED_FLAGS_ID;
        entity.getEntityData().set(flags,(byte)11);
        if(entity.getEntityData().get(flags)!=(byte)11)throw new AssertionError("Metadata flag read failed");
        io.netty.buffer.ByteBuf original=io.netty.buffer.Unpooled.buffer();
        org.basaltmc.bukkit.net.PacketCodec.varInt(original,42);
        original.writeByte(0).writeByte(0).writeByte(11);
        original.writeByte(1).writeByte(1);org.basaltmc.bukkit.net.PacketCodec.varInt(original,123);
        original.writeByte(2).writeByte(5);org.basaltmc.bukkit.net.PacketCodec.component(original,net.minecraft.network.chat.Component.literal("name"));
        original.writeByte(255);
        byte[] bytes=new byte[original.readableBytes()];original.readBytes(bytes);original.release();
        var decoded=(net.minecraft.network.protocol.game.ClientboundSetEntityDataPacket)org.basaltmc.bukkit.net.PacketCodec.fromNative(0x65,bytes);
        var handler = new io.netty.channel.ChannelOutboundHandlerAdapter() {
            @Override public void write(io.netty.channel.ChannelHandlerContext context,Object message,io.netty.channel.ChannelPromise promise) {
                if(message instanceof net.minecraft.network.protocol.game.ClientboundSetEntityDataPacket packet) {
                    var values=new java.util.ArrayList<net.minecraft.network.syncher.SynchedEntityData.DataValue<?>>(packet.packedItems());
                    values.set(0,net.minecraft.network.syncher.SynchedEntityData.DataValue.create(flags,(byte)75));
                    message=new net.minecraft.network.protocol.game.ClientboundSetEntityDataPacket(packet.id(),values);
                }
                context.write(message,promise);
            }
        };
        var channel=new io.netty.channel.embedded.EmbeddedChannel(new org.basaltmc.bukkit.net.WirePacketEncoder(),handler);
        new net.minecraft.server.network.ServerCommonPacketListenerImpl(new net.minecraft.network.Connection(channel)).send(decoded);
        if(entity.getEntityData().get(flags)!=(byte)11)throw new AssertionError("Per-viewer rewrite changed global flags");
        io.netty.buffer.ByteBuf frame=channel.readOutbound();
        try {byte[] result=new byte[frame.readableBytes()];frame.readBytes(result);return result;}
        finally{frame.release();channel.finishAndReleaseAll();}
    }

    public static void failureText() {
        Throwable failure = new IllegalStateException("outer probe", new IllegalArgumentException("nested probe"));
        failure.addSuppressed(new RuntimeException("suppressed probe"));
        String trace = org.basaltmc.bukkit.BukkitRuntime.describeFailure(failure);
        if (!trace.contains("IllegalStateException: outer probe") || !trace.contains("Caused by:")
                || !trace.contains("IllegalArgumentException: nested probe")
                || !trace.contains("Suppressed:") || !trace.contains("RuntimeException: suppressed probe")) {
            throw new AssertionError("Diagnostic trace lost exception context");
        }
    }

    public static void loggingBackend() {
        System.setProperty("log4j2.loggerContextFactory", "org.basaltmc.bukkit.log.BasaltLog4jFactory");
        org.apache.logging.log4j.Logger api = org.apache.logging.log4j.LogManager.getRootLogger();
        if (!(api instanceof org.apache.logging.log4j.core.Logger root)) {
            throw new AssertionError("Root logger has no shared Log4j backend");
        }
        int[] received = {0};
        org.apache.logging.log4j.core.Appender appender =
                new org.apache.logging.log4j.core.appender.AbstractAppender("compatibility-probe", null, null, false,
                        org.apache.logging.log4j.core.config.Property.EMPTY_ARRAY) {
                    @Override public void append(org.apache.logging.log4j.core.LogEvent event) { received[0]++; }
                };
        appender.start();
        root.addAppender(appender);
        try {
            root.error("Runtime compatibility logging probe");
            if (received[0] != 1) throw new AssertionError("Appender did not receive the logging event");
        } finally {
            root.removeAppender(appender);
            appender.stop();
        }
        org.apache.logging.log4j.LogManager.shutdown();
    }
}

// Deliberately removed after compilation: optional integration types need not be installed.
class MissingDependency {}
