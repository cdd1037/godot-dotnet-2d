using Godot;
using System;
using System.Runtime.CompilerServices;
using System.Security.Cryptography;
using System.Text;
using System.Globalization;
using System.Threading.Tasks;

[assembly: RegisterScriptType(typeof(SmokeRef))]
[assembly: RegisterScriptType(typeof(SmokeGeneric<int>))]

public partial class Main : Node2D
{
    [Export]
    public int ProbeValue { get; set; } = 7;

    public int NativeProbe(int value) => ProbeValue + value;

    [Signal]
    public delegate void AnswerEventHandler(int value);

    private static void ValidateCryptography()
    {
        byte[] plain = Encoding.UTF8.GetBytes("Godot Android Mono trimming 中文");
        byte[] hash = SHA256.HashData(Encoding.ASCII.GetBytes("abc"));
        if (Convert.ToHexString(hash) != "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD")
            throw new Exception("SHA256 known vector failed");
        byte[] key = System.Security.Cryptography.RandomNumberGenerator.GetBytes(32);
        byte[] nonce = System.Security.Cryptography.RandomNumberGenerator.GetBytes(12);
        byte[] ciphertext = new byte[plain.Length];
        byte[] tag = new byte[16];
        byte[] recovered = new byte[plain.Length];
        using (var aes = new AesGcm(key, tag.Length))
        {
            aes.Encrypt(nonce, plain, ciphertext, tag);
            aes.Decrypt(nonce, ciphertext, tag, recovered);
        }
        if (!CryptographicOperations.FixedTimeEquals(plain, recovered))
            throw new Exception("AES-GCM round trip failed");
        byte[] derived = Rfc2898DeriveBytes.Pbkdf2("password", Encoding.ASCII.GetBytes("salt"), 1, HashAlgorithmName.SHA256, 32);
        if (Convert.ToHexString(derived) != "120FB6CFFCF8B32C43E7225256C4F837A86548C92CCC35480805987CB70BE17B")
            throw new Exception("PBKDF2 known vector failed");
        using var rsa = RSA.Create(2048);
        byte[] signature = rsa.SignData(plain, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
        if (!rsa.VerifyData(plain, signature, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1))
            throw new Exception("RSA signature round trip failed");
        GD.Print("ANDROID_MONO_CRYPTO_SMOKE_OK");
    }

    public override async void _Ready()
    {
        try
        {
            Set(PropertyName.ProbeValue, 19);
            if (Get(PropertyName.ProbeValue).AsInt32() != 19 || Call(MethodName.NativeProbe, 4).AsInt32() != 23)
                throw new Exception("Exported property or native method dispatch failed");
            Position = new Vector2(12, 34);
            if (Position != new Vector2(12, 34)) throw new Exception("Node2D binding failed");
            var texture = ResourceLoader.Load<Texture2D>("res://probe.png");
            if (texture is null || texture.GetWidth() != 8 || texture.GetHeight() != 8)
                throw new Exception("ETC2 texture import/load failed");
            AddChild(new Sprite2D { Texture = texture });
            var control = new Control();
            AddChild(control);
            control.Size = new Vector2(32, 16);
            if (control.Size.X != 32) throw new Exception("Control binding failed");
            var world = GetWorld2D();
            if (!world.Space.IsValid) throw new Exception("World2D physics space unavailable");
            _ = world.DirectSpaceState;
            var tiles = new TileMapLayer();
            AddChild(tiles);
            tiles.NavigationEnabled = false;
            var shape = new RectangleShape2D { Size = new Vector2(4, 8) };
            if (shape.Size.Y != 8) throw new Exception("2D shape binding failed");
            shape.Dispose();
            int answer = 0;
            Answer += value => answer = value;
            EmitSignal(SignalName.Answer, 42);
            if (answer != 42) throw new Exception("Generated signal dispatch failed");
            if (ClassDB.ClassExists("Node3D")) throw new Exception("Unexpected Node3D API");
            if (Array.IndexOf(OS.GetCmdlineUserArgs(), "--minimal-template") >= 0)
            {
                if (ClassDB.ClassExists("FastNoiseLite") || ClassDB.ClassExists("RegEx"))
                    throw new Exception("Unexpected optional module in minimal template");
            }
            using var uniform = new RDUniform();
            using var script = new SmokeRef();
            using var generic = new SmokeGeneric<int>();
            var nativeValues = new Godot.Collections.Array<RDUniform> { uniform };
            using var nativeValuesOwner = (Godot.Collections.Array)nativeValues;
            var scriptValues = new Godot.Collections.Dictionary<SmokeRef, SmokeGeneric<int>> { [script] = generic };
            using var scriptValuesOwner = (Godot.Collections.Dictionary)scriptValues;
            if (nativeValues[0].GetInstanceId() != uniform.GetInstanceId() ||
                scriptValues[script].GetInstanceId() != generic.GetInstanceId() ||
                generic.Call("Marker").AsInt32() != 31)
                throw new Exception("Typed native/script collections or closed generic metadata failed");
            bool expectAot = Array.IndexOf(OS.GetCmdlineUserArgs(), "--expect-aot") >= 0;
            if (expectAot && RuntimeFeature.IsDynamicCodeSupported)
                throw new Exception("NativeAOT export unexpectedly contains a JIT runtime");
            GD.Print($"CI_DOTNET_RUNTIME dynamic_code={RuntimeFeature.IsDynamicCodeSupported}");
            if (!RuntimeFeature.IsDynamicCodeSupported)
                throw new Exception("Android Mono validation requires the JIT runtime");
            ValidateCryptography();
            await Task.Run(ValidateCryptography);
            await ToSignal(GetTree().CreateTimer(0.01), SceneTreeTimer.SignalName.Timeout);
            var culture = CultureInfo.GetCultureInfo("fr-FR");
            if (12.5m.ToString("F1", culture) != "12,5")
                throw new Exception("Normal globalization failed");
            GD.Print("ANDROID_MONO_TRIM_SMOKE_OK");
            GetTree().Quit(0);
        }
        catch (Exception exception)
        {
            GD.PushError(exception.ToString());
            GetTree().Quit(1);
        }
    }
}


[NoScriptFileAssociation]
public partial class SmokeRef : RefCounted
{
}

[NoScriptFileAssociation]
public partial class SmokeGeneric<T> : RefCounted
{
    public int Marker() => 31;
}

// The payload inspector requires this dead type to disappear only in trimmed mode.
internal sealed class AndroidTrimUnusedSentinel
{
    internal static string NeverCalled() => "This method must not be rooted by the hosted entrypoint.";
}
