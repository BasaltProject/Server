package compatibility;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.Statement;
import net.minecraft.resources.ResourceKey;

public final class RuntimeCompatibilityTest {
    public static final class OptionalIntegration {
        private MissingDependency optionalImplementation() { return null; }
        public static String available() { return "available"; }
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
