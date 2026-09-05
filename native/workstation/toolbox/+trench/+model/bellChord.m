function chord = bellChord(words)
chord = trench.bridge.decompile(words);
on = chord(:,1) ~= 0;
chord(on,3) = 0.25;
chord(on,4) = 1;
chord(on,5) = chord(on,2);
chord(on,6) = 4;
chord(6,:) = [0 60 12 1 trench.bridge.noteOf(20000) 0 chord(6,7)];
chord = trench.bridge.fitVoices(chord);
end
