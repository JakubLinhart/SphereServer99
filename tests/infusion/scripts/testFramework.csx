#load "startup.csx"

using System;

public void Test(string name, Action testAction)
{
    UO.Console.Info("");
    UO.Console.Important($"Starting {name}...");

    Startup.RunInGame(testAction);
}

public void ExecuteCommand(string cmd)
{
    UO.Say("." + cmd);
    UO.Journal.WaitAny(TimeSpan.FromSeconds(1), cmd + " done");
}

public void Success(string text)
{
    UO.Console.Info("        OK - " + text);
}

public void Fail(string text)
{
    UO.Console.Error("    FAILED - " + text);
}

public void ShouldBeTrue(string comment, bool result)
{
    if (result)
        Success(comment);
    else
        Fail(comment);
}