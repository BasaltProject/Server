// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.methods;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.server.jsonrpc.JsonRPCUtils;
import net.minecraft.server.jsonrpc.api.MethodInfo;
import net.minecraft.server.jsonrpc.api.Schema;
import net.minecraft.server.jsonrpc.api.SchemaComponent;

public class DiscoveryService {
    public static DiscoverResponse discover(List<SchemaComponent<?>> schemaRegistry) {
        net.minecraft.server.jsonrpc.IncomingRpcMethods.ensureRegistered();
        List<MethodInfo.Named<?, ?>> methods = new ArrayList<>();
        BuiltInRegistries.INCOMING_RPC_METHOD.listElements().filter(entry -> entry.value().attributes().discoverable())
            .forEach(entry -> methods.add(entry.value().info().named(entry.key().identifier())));
        BuiltInRegistries.OUTGOING_RPC_METHOD.listElements().filter(entry -> entry.value().attributes().discoverable())
            .forEach(entry -> methods.add(entry.value().info().named(entry.key().identifier())));
        Map<String, Schema<?>> schemas = new LinkedHashMap<>();
        for (SchemaComponent<?> component : schemaRegistry) {
            schemas.put(component.name(), component.schema().info());
        }
        return new DiscoverResponse(JsonRPCUtils.OPEN_RPC_VERSION, new DiscoverInfo("Minecraft Server JSON-RPC", "3.1.0"), methods,
            new DiscoverComponents(schemas));
    }

    public static record DiscoverResponse(String jsonRpcProtocolVersion, DiscoverInfo discoverInfo, List<MethodInfo.Named<?, ?>> methods,
        DiscoverComponents components) {
        public static final MapCodec<DiscoverResponse> CODEC = RecordCodecBuilder.mapCodec(group -> group.group(
            Codec.STRING.fieldOf("openrpc").forGetter(DiscoverResponse::jsonRpcProtocolVersion),
            DiscoverInfo.CODEC.codec().fieldOf("info").forGetter(DiscoverResponse::discoverInfo),
            MethodInfo.Named.CODEC.listOf().fieldOf("methods").forGetter(DiscoverResponse::methods),
            DiscoverComponents.CODEC.codec().fieldOf("components").forGetter(DiscoverResponse::components)).apply(group, DiscoverResponse::new));
    }

    public static record DiscoverComponents(Map<String, Schema<?>> schemas) {
        @SuppressWarnings({"unchecked", "rawtypes"})
        public static final MapCodec<DiscoverComponents> CODEC = RecordCodecBuilder.mapCodec(group -> group.group(
            Codec.unboundedMap(Codec.STRING, (Codec<Schema<?>>) (Codec) Schema.CODEC).fieldOf("schemas")
                .forGetter(DiscoverComponents::schemas)).apply(group, DiscoverComponents::new));
    }

    public static record DiscoverInfo(String title, String version) {
        public static final MapCodec<DiscoverInfo> CODEC = RecordCodecBuilder.mapCodec(group -> group.group(
            Codec.STRING.fieldOf("title").forGetter(DiscoverInfo::title),
            Codec.STRING.fieldOf("version").forGetter(DiscoverInfo::version)).apply(group, DiscoverInfo::new));
    }
}
