// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.api;

import com.mojang.datafixers.util.Either;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.DynamicOps;
import com.mojang.serialization.RecordBuilder;
import java.net.URI;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.function.Supplier;
import net.minecraft.core.UUIDUtil;
import net.minecraft.server.jsonrpc.methods.BanlistService;
import net.minecraft.server.jsonrpc.methods.DiscoveryService;
import net.minecraft.server.jsonrpc.methods.GameRulesService;
import net.minecraft.server.jsonrpc.methods.IpBanlistService;
import net.minecraft.server.jsonrpc.methods.Message;
import net.minecraft.server.jsonrpc.methods.OperatorService;
import net.minecraft.server.jsonrpc.methods.PlayerService;
import net.minecraft.server.jsonrpc.methods.ServerStateService;
import net.minecraft.server.permissions.PermissionLevel;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.gamerules.GameRuleType;

public record Schema<T>(Optional<URI> reference, List<String> type, Optional<Schema<?>> items, Map<String, Schema<?>> properties,
    List<String> enumValues, Codec<T> codec) {
    private static final List<SchemaComponent<?>> COMPONENTS = new ArrayList<>();

    /** Write-only: a schema is described to clients, never read back from them. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static final Codec<? extends Schema<?>> CODEC = (Codec) Codec.of(new com.mojang.serialization.Encoder<Schema<?>>() {
        @Override
        public <O> DataResult<O> encode(Schema<?> schema, DynamicOps<O> ops, O prefix) {
            RecordBuilder<O> out = ops.mapBuilder();
            schema.reference.ifPresent(ref -> out.add("$ref", ops.createString(ref.toString())));
            if (!schema.type.isEmpty()) {
                out.add("type", schema.type.size() == 1 ? ops.createString(schema.type.get(0))
                    : ops.createList(schema.type.stream().map(ops::createString)));
            }
            schema.items.ifPresent(item -> out.add("items", encode(item, ops, ops.empty())));
            if (!schema.properties.isEmpty()) {
                Map<O, O> fields = new LinkedHashMap<>();
                schema.properties.forEach((key, value) -> fields.put(ops.createString(key), encode(value, ops, ops.empty()).result().orElse(ops.empty())));
                out.add("properties", ops.createMap(fields));
            }
            if (!schema.enumValues.isEmpty()) {
                out.add("enum", ops.createList(schema.enumValues.stream().map(ops::createString)));
            }
            return out.build(prefix);
        }
    }, new com.mojang.serialization.Decoder<Schema<?>>() {
        @Override
        public <O> DataResult<Pair<Schema<?>, O>> decode(DynamicOps<O> ops, O input) {
            return DataResult.error(() -> "Should not deserialize schema");
        }
    });

    public static final Schema<Boolean> BOOL_SCHEMA = ofType("boolean", Codec.BOOL);
    public static final Schema<Integer> INT_SCHEMA = ofType("integer", Codec.INT);
    public static final Schema<Either<Boolean, Integer>> BOOL_OR_INT_SCHEMA = ofTypes(List.of("boolean", "integer"), Codec.either(Codec.BOOL, Codec.INT));
    public static final Schema<Float> NUMBER_SCHEMA = ofType("number", Codec.FLOAT);
    public static final Schema<String> STRING_SCHEMA = ofType("string", Codec.STRING);
    public static final Schema<UUID> UUID_SCHEMA = ofType("string", UUIDUtil.CODEC);
    public static final Schema<DiscoveryService.DiscoverResponse> DISCOVERY_SCHEMA = ofType("string", Codec.lazyInitialized(() -> DiscoveryService.DiscoverResponse.CODEC.codec()));
    public static final SchemaComponent<Difficulty> DIFFICULTY_SCHEMA = component("difficulty", ofEnum(Difficulty::values, Difficulty.CODEC));
    public static final SchemaComponent<GameType> GAME_TYPE_SCHEMA = component("game_type", ofEnum(GameType::values, GameType.CODEC));
    public static final Schema<PermissionLevel> PERMISSION_LEVEL_SCHEMA = ofType("integer", PermissionLevel.INT_CODEC);
    public static final SchemaComponent<PlayerDto> PLAYER_SCHEMA = component("player",
        record(PlayerDto.CODEC.codec()).withField("id", UUID_SCHEMA).withField("name", STRING_SCHEMA));
    public static final SchemaComponent<DiscoveryService.DiscoverInfo> VERSION_SCHEMA = component("version",
        record(DiscoveryService.DiscoverInfo.CODEC.codec()).withField("name", STRING_SCHEMA).withField("protocol", INT_SCHEMA));
    public static final SchemaComponent<ServerStateService.ServerState> SERVER_STATE_SCHEMA = component("server_state",
        record(ServerStateService.ServerState.CODEC).withField("started", BOOL_SCHEMA)
            .withField("players", PLAYER_SCHEMA.asArray()).withField("version", VERSION_SCHEMA.asRef()));
    public static final Schema<GameRuleType> RULE_TYPE_SCHEMA = ofEnum(GameRuleType::values);
    public static final SchemaComponent<GameRulesService.GameRuleUpdate<?>> TYPED_GAME_RULE_SCHEMA = component("typed_game_rule",
        record(GameRulesService.GameRuleUpdate.TYPED_CODEC).withField("key", STRING_SCHEMA)
            .withField("value", BOOL_OR_INT_SCHEMA).withField("type", RULE_TYPE_SCHEMA));
    public static final SchemaComponent<GameRulesService.GameRuleUpdate<?>> UNTYPED_GAME_RULE_SCHEMA = component("untyped_game_rule",
        record(GameRulesService.GameRuleUpdate.CODEC).withField("key", STRING_SCHEMA).withField("value", BOOL_OR_INT_SCHEMA));
    public static final SchemaComponent<Message> MESSAGE_SCHEMA = component("message",
        record(Message.CODEC).withField("literal", STRING_SCHEMA).withField("translatable", STRING_SCHEMA)
            .withField("translatableParams", STRING_SCHEMA.asArray()));
    public static final SchemaComponent<ServerStateService.SystemMessage> SYSTEM_MESSAGE_SCHEMA = component("system_message",
        record(ServerStateService.SystemMessage.CODEC).withField("message", MESSAGE_SCHEMA.asRef())
            .withField("overlay", BOOL_SCHEMA).withField("receivingPlayers", PLAYER_SCHEMA.asArray()));
    public static final SchemaComponent<PlayerService.KickDto> KICK_PLAYER_SCHEMA = component("kick_player",
        record(PlayerService.KickDto.CODEC.codec()).withField("message", MESSAGE_SCHEMA.asRef()).withField("player", PLAYER_SCHEMA.asRef()));
    public static final SchemaComponent<OperatorService.OperatorDto> OPERATOR_SCHEMA = component("operator",
        record(OperatorService.OperatorDto.CODEC.codec()).withField("player", PLAYER_SCHEMA.asRef())
            .withField("bypassesPlayerLimit", BOOL_SCHEMA).withField("permissionLevel", INT_SCHEMA));
    public static final SchemaComponent<IpBanlistService.IncomingIpBanDto> INCOMING_IP_BAN_SCHEMA = component("incoming_ip_ban",
        record(IpBanlistService.IncomingIpBanDto.CODEC.codec()).withField("player", PLAYER_SCHEMA.asRef()).withField("ip", STRING_SCHEMA)
            .withField("reason", STRING_SCHEMA).withField("source", STRING_SCHEMA).withField("expires", STRING_SCHEMA));
    public static final SchemaComponent<IpBanlistService.IpBanDto> IP_BAN_SCHEMA = component("ip_ban",
        record(IpBanlistService.IpBanDto.CODEC.codec()).withField("ip", STRING_SCHEMA).withField("reason", STRING_SCHEMA)
            .withField("source", STRING_SCHEMA).withField("expires", STRING_SCHEMA));
    public static final SchemaComponent<BanlistService.UserBanDto> PLAYER_BAN_SCHEMA = component("user_ban",
        record(BanlistService.UserBanDto.CODEC.codec()).withField("player", PLAYER_SCHEMA.asRef()).withField("reason", STRING_SCHEMA)
            .withField("source", STRING_SCHEMA).withField("expires", STRING_SCHEMA));

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T> Codec<Schema<T>> typedCodec() {
        return (Codec) CODEC;
    }

    public Schema<T> info() {
        Map<String, Schema<?>> described = new LinkedHashMap<>();
        properties.forEach((key, value) -> described.put(key, value.info()));
        return new Schema<>(reference, type, items.map(Schema::info), described, enumValues, codec);
    }

    private static <T> SchemaComponent<T> component(String name, Schema<T> schema) {
        SchemaComponent<T> entry = new SchemaComponent<>(name, ReferenceUtil.createLocalReference(name), schema);
        COMPONENTS.add(entry);
        return entry;
    }

    public static List<SchemaComponent<?>> getSchemaRegistry() {
        return COMPONENTS;
    }

    public static <T> Schema<T> ofRef(URI ref, Codec<T> codec) {
        return new Schema<>(Optional.of(ref), List.of(), Optional.empty(), Map.of(), List.of(), codec);
    }

    public static <T> Schema<T> ofType(String type, Codec<T> codec) {
        return ofTypes(List.of(type), codec);
    }

    public static <T> Schema<T> ofTypes(List<String> types, Codec<T> codec) {
        return new Schema<>(Optional.empty(), types, Optional.empty(), Map.of(), List.of(), codec);
    }

    public static <E extends Enum<E> & StringRepresentable> Schema<E> ofEnum(Supplier<E[]> values) {
        return ofEnum(values, StringRepresentable.fromEnum(values));
    }

    public static <E extends Enum<E> & StringRepresentable> Schema<E> ofEnum(Supplier<E[]> values, Codec<E> codec) {
        List<String> names = new ArrayList<>();
        for (E constant : values.get()) {
            names.add(constant.getSerializedName());
        }
        return ofEnum(names, codec);
    }

    public static <T> Schema<T> ofEnum(List<String> enumValues, Codec<T> codec) {
        return new Schema<>(Optional.empty(), List.of("string"), Optional.empty(), Map.of(), enumValues, codec);
    }

    public static <T> Schema<List<T>> arrayOf(Schema<?> item, Codec<T> codec) {
        return new Schema<>(Optional.empty(), List.of("array"), Optional.of(item), Map.of(), List.of(), codec.listOf());
    }

    public static <T> Schema<T> record(Codec<T> codec) {
        return new Schema<>(Optional.empty(), List.of("object"), Optional.empty(), Map.of(), List.of(), codec);
    }

    public Schema<T> withField(String name, Schema<?> field) {
        Map<String, Schema<?>> merged = new LinkedHashMap<>(properties);
        merged.put(name, field);
        return new Schema<>(Optional.empty(), List.of("object"), Optional.empty(), merged, List.of(), codec);
    }

    public Schema<List<T>> asArray() {
        return arrayOf(this, codec);
    }
}
