function chord = bellChord(words)
chord = trench.bridge.decompile(words);
on = chord(:,1) ~= 0; on(6) = false;
chord(on,3) = 0.25;
chord(on,4) = 1;
chord(on,5) = chord(on,2);
chord(on,6) = 4;
corner = 100; if any(on), corner = trench.bridge.hzOf(min(chord(on,2)))/2; end
chord(6,:) = trench.model.shelfRow(0,corner,chord(6,7));
chord = trench.bridge.fitVoices(chord);
end
