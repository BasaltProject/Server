// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.security;

import io.netty.channel.ChannelDuplexHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelPromise;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.List;

/** The handshake itself is native; this checks keys and origins the same way for plugins that call it. */
public class AuthenticationHandler extends ChannelDuplexHandler {
    public static final String BEARER_PREFIX = "Bearer ";
    private final SecurityConfig securityConfig;
    private final List<String> allowedOrigins;

    public AuthenticationHandler(SecurityConfig securityConfig, String allowedOrigins) {
        this.securityConfig = securityConfig;
        this.allowedOrigins = Arrays.stream(allowedOrigins.split(",")).map(String::trim).filter(origin -> !origin.isEmpty()).toList();
    }

    public void channelRead(ChannelHandlerContext context, Object msg) throws Exception {
        context.fireChannelRead(msg);
    }

    public void write(ChannelHandlerContext ctx, Object msg, ChannelPromise promise) throws Exception {
        ctx.write(msg, promise);
    }

    public boolean isValidApiKey(String suppliedKey) {
        if (suppliedKey == null || securityConfig == null) {
            return false;
        }
        return MessageDigest.isEqual(suppliedKey.getBytes(StandardCharsets.UTF_8), securityConfig.secretKey().getBytes(StandardCharsets.UTF_8));
    }

    boolean isOriginAllowed(String origin) {
        return origin != null && allowedOrigins.contains(origin);
    }

    static class SecurityCheckResult {
        private final boolean allowed;
        private final String reason;
        private final boolean tokenSentInSecWebsocketProtocol;

        private SecurityCheckResult(boolean allowed, String reason, boolean tokenSentInSecWebsocketProtocol) {
            this.allowed = allowed;
            this.reason = reason;
            this.tokenSentInSecWebsocketProtocol = tokenSentInSecWebsocketProtocol;
        }

        public static SecurityCheckResult allowed() {
            return new SecurityCheckResult(true, null, false);
        }

        public static SecurityCheckResult allowed(boolean tokenSentInSecWebsocketProtocol) {
            return new SecurityCheckResult(true, null, tokenSentInSecWebsocketProtocol);
        }

        public static SecurityCheckResult denied(String reason) {
            return new SecurityCheckResult(false, reason, false);
        }

        public boolean isAllowed() {
            return allowed;
        }

        public String getReason() {
            return reason;
        }

        public boolean isTokenSentInSecWebsocketProtocol() {
            return tokenSentInSecWebsocketProtocol;
        }
    }
}
