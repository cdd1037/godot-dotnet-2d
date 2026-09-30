using Godot;
using System;
using System.Linq;
using System.Threading.Tasks;

public partial class Main : Node2D
{
    private Rid _navMap, _navRegion;
    private void Check(bool ok, string name)
    {
        if (!ok) throw new Exception(name);
        GD.Print("PASS: " + name);
    }
    public override async void _Ready()
    {
        try
        {
            bool requirePruned = OS.GetCmdlineUserArgs().Contains("--require-pruned");
            InteropRegressionCases.Run(this);
            Check(!ClassDB.ClassExists("GDScript"), "GDScript absent");
            if (requirePruned)
            {
                string[] absent = { "Node3D", "Camera3D", "PhysicsServer3D", "NavigationServer3D", "XRServer", "XRInterface", "World3D", "Environment", "StandardMaterial3D", "VideoStreamPlayer", "VideoStreamTheora", "SceneMultiplayer", "MultiplayerAPI", "ENetMultiplayerPeer", "WebSocketPeer", "WebRTCPeerConnection", "UPNP", "JSONRPC", "ZIPReader", "ZIPPacker" };
                foreach (string type in absent) Check(!ClassDB.ClassExists(type), type + " absent");
                foreach (string type in new[] { "Node3D", "Camera3D", "RpcAttribute", "MultiplayerApi", "VideoStreamPlayer" })
                    Check(typeof(Node).Assembly.GetType("Godot." + type) == null, "managed " + type + " absent");
                Check(!typeof(Node).GetMethods().Any(m => m.Name == "Rpc" || m.Name == "RpcId"), "managed Node RPC absent");
                Check(!ClassDB.ClassHasMethod("Node", "rpc"), "Node RPC absent");
                Check(!ClassDB.ClassHasMethod("RenderingServer", "camera_create"), "3D camera API absent");
            }
            foreach (string type in new[] { "Node2D", "Sprite2D", "PhysicsServer2D", "NavigationServer2D", "FastNoiseLite", "ParticleProcessMaterial", "GPUParticles2D", "VisualShader", "MovieWriter", "HTTPRequest", "StreamPeerTCP", "RenderingDevice" })
                Check(ClassDB.ClassExists(type), type + " available");

            var image = Image.CreateEmpty(8, 8, false, Image.Format.Rgba8);
            image.Fill(Colors.Red);
            var texture = ImageTexture.CreateFromImage(image);
            AddChild(new Sprite2D { Texture = texture, Position = new Vector2(160, 120), Scale = new Vector2(20, 20) });
            var body = new RigidBody2D { Position = new Vector2(160, 20) };
            body.AddChild(new CollisionShape2D { Shape = new CircleShape2D { Radius = 8 } });
            AddChild(body);
            var anim = new Animation();
            int track = anim.AddTrack(Animation.TrackType.Value);
            anim.TrackSetPath(track, "Sprite2D:position");
            anim.TrackInsertKey(track, 0, Vector2.Zero);
            anim.TrackInsertKey(track, 1, new Vector2(20, 40));
            var interpolated = anim.ValueTrackInterpolate(track, 0.5).AsVector2();
            Check(interpolated.IsEqualApprox(new Vector2(10, 20)), "Vector2 animation interpolation");
            Check(!float.IsNaN(new FastNoiseLite().GetNoise2D(3, 7)), "noise sample");

            _navMap = NavigationServer2D.MapCreate();
            NavigationServer2D.MapSetCellSize(_navMap, 1);
            NavigationServer2D.MapSetActive(_navMap, true);
            var polygon = new NavigationPolygon { CellSize = 1, Vertices = new[] { Vector2.Zero, new Vector2(100, 0), new Vector2(100, 100), new Vector2(0, 100) } };
            polygon.AddPolygon(new[] { 0, 1, 2, 3 });
            _navRegion = NavigationServer2D.RegionCreate();
            NavigationServer2D.RegionSetMap(_navRegion, _navMap);
            NavigationServer2D.RegionSetNavigationPolygon(_navRegion, polygon);
            for (int i = 0; i < 60 && NavigationServer2D.MapGetIterationId(_navMap) == 0; i++) await ToSignal(GetTree(), SceneTree.SignalName.PhysicsFrame);
            Check(NavigationServer2D.MapGetIterationId(_navMap) > 0, "navigation map synchronized");
            Vector2[] path = Array.Empty<Vector2>();
            for (int i = 0; i < 120 && path.Length < 2; i++)
            {
                await ToSignal(GetTree(), SceneTree.SignalName.PhysicsFrame);
                if (NavigationServer2D.RegionGetIterationId(_navRegion) > 0)
                    path = NavigationServer2D.MapGetPath(_navMap, new Vector2(10, 10), new Vector2(80, 80), true);
            }
            GD.Print($"Navigation diagnostics: map={NavigationServer2D.MapGetIterationId(_navMap)}, region={NavigationServer2D.RegionGetIterationId(_navRegion)}, polygons={polygon.GetPolygonCount()}, points={path.Length}");
            Check(path.Length >= 2, "2D navigation path");

            for (int i = 0; i < 12; i++) await ToSignal(GetTree(), SceneTree.SignalName.PhysicsFrame);
            Check(body.Position.Y > 20, "2D rigid-body simulation");
            if (DisplayServer.GetName() != "headless")
            {
                GetViewport().UseHdr2D = true;
                AddChild(new CanvasModulate { Color = new Color(0.1f, 0.1f, 0.1f, 1) });
                var lightImage = Image.CreateEmpty(8, 8, false, Image.Format.Rgba8);
                lightImage.Fill(Colors.White);
                var light = new PointLight2D { Texture = ImageTexture.CreateFromImage(lightImage), TextureScale = 60, Position = new Vector2(80, 120), Energy = 2.0f, Enabled = false };
                AddChild(light);
                var particles = new GpuParticles2D { Amount = 32, Position = new Vector2(280, 40), Texture = texture, Emitting = true };
                particles.ProcessMaterial = new ParticleProcessMaterial { ParticleFlagDisableZ = true, Gravity = Vector3.Zero, InitialVelocityMin = 10, InitialVelocityMax = 20 };
                AddChild(particles);
                CheckCompute();
                var dark = await Capture("validation-dark.png");
                float darkRed = dark.GetPixel(180, 120).R;
                light.Enabled = true;
                for (int i = 0; i < 20; i++) await ToSignal(RenderingServer.Singleton, RenderingServer.SignalName.FramePostDraw);
                var lit = await Capture("validation-lit.png");
                float litRed = lit.GetPixel(180, 120).R;
                GD.Print($"Lighting diagnostic: dark={darkRed}, lit={litRed}, format={lit.GetFormat()}");
                Check(litRed > darkRed + 0.3f, "PointLight2D changes pixel brightness");
                Check(lit.GetFormat() == Image.Format.Rgbh || lit.GetFormat() == Image.Format.Rgbf || lit.GetFormat() == Image.Format.Rgbah || lit.GetFormat() == Image.Format.Rgbaf, "HDR2D floating-point render target");
                Check(litRed > 1.0f, "HDR2D preserves overbright values");
                int particlePixels = 0;
                for (int y = 10; y < 75; y++) for (int x = 250; x < 315; x++)
                {
                    Color pixel = lit.GetPixel(x, y);
                    if (pixel.R > 0.2f && pixel.G < 0.15f && pixel.B < 0.15f) particlePixels++;
                }
                Check(particlePixels > 4, "GPU particles produce visible pixels");
                var occluder = new LightOccluder2D { Position = new Vector2(120, 120), Occluder = new OccluderPolygon2D { Polygon = new[] { new Vector2(-4, -60), new Vector2(4, -60), new Vector2(4, 60), new Vector2(-4, 60) }, CullMode = OccluderPolygon2D.CullModeEnum.Disabled } };
                AddChild(occluder);
                light.ShadowEnabled = true;
                var shadow = await Capture("validation-shadow.png");
                float shadowRed = shadow.GetPixel(180, 120).R;
                GD.Print($"Shadow diagnostic: unoccluded={litRed}, shadow={shadowRed}");
                Check(shadowRed < litRed - 0.3f, "LightOccluder2D produces a shadow");
                GetViewport().Msaa2D = Viewport.Msaa.Msaa2X;
                var msaa = await Capture("validation-msaa.png");
                Check(!msaa.IsEmpty(), "2D MSAA render/readback");
            }
            else
            {
                if (OS.GetCmdlineUserArgs().Contains("--compute")) CheckCompute();
                GD.Print("SKIP: windowed drawing checks in headless mode");
            }
            GD.Print("FORK_VALIDATION_OK");
            NavigationServer2D.FreeRid(_navRegion); NavigationServer2D.FreeRid(_navMap);
            _navRegion = default; _navMap = default;
            GetTree().Quit();
        }
        catch (Exception ex) { GD.PushError(ex.ToString()); GetTree().Quit(1); }
    }
    private async Task<Image> Capture(string name)
    {
        for (int i = 0; i < 4; i++) await ToSignal(RenderingServer.Singleton, RenderingServer.SignalName.FramePostDraw);
        var image = GetViewport().GetTexture().GetImage();
        Check(!image.IsEmpty(), "Vulkan frame readback " + name);
        Check(image.SavePng("user://" + name) == Error.Ok, "saved " + name);
        return image;
    }
    public override void _ExitTree()
    {
        if (_navRegion.IsValid) NavigationServer2D.FreeRid(_navRegion);
        if (_navMap.IsValid) NavigationServer2D.FreeRid(_navMap);
    }
    private void CheckCompute()
    {
        var rd = RenderingServer.CreateLocalRenderingDevice();
        Check(rd != null, "local RenderingDevice available");
        var source = new RDShaderSource { SourceCompute = "#version 450\nlayout(local_size_x=1) in; layout(set=0,binding=0,std430) buffer Data { uint value; }; void main(){value=42;}" };
        var spirv = rd.ShaderCompileSpirVFromSource(source);
        Check(string.IsNullOrEmpty(spirv.CompileErrorCompute), "compute shader compile");
        Rid shader = rd.ShaderCreateFromSpirV(spirv);
        Rid buffer = rd.StorageBufferCreate(4, new byte[4]);
        var uniform = new RDUniform { UniformType = RenderingDevice.UniformType.StorageBuffer, Binding = 0 };
        uniform.AddId(buffer);
        Rid set = rd.UniformSetCreate(new Godot.Collections.Array<RDUniform> { uniform }, shader, 0);
        Rid pipeline = rd.ComputePipelineCreate(shader);
        long list = rd.ComputeListBegin();
        rd.ComputeListBindComputePipeline(list, pipeline); rd.ComputeListBindUniformSet(list, set, 0); rd.ComputeListDispatch(list, 1, 1, 1); rd.ComputeListEnd();
        rd.Submit(); rd.Sync();
        Check(BitConverter.ToUInt32(rd.BufferGetData(buffer), 0) == 42, "compute dispatch/readback");
        rd.FreeRid(set); rd.FreeRid(pipeline); rd.FreeRid(shader); rd.FreeRid(buffer); rd.Free();
    }
}
