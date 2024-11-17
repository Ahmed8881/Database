import { Component, OnInit, Inject, PLATFORM_ID } from '@angular/core';
import { isPlatformBrowser } from '@angular/common';

@Component({
  selector: 'app-hero',
  standalone: true,
  imports: [],
  templateUrl: './hero.component.html',
  styleUrls: ['./hero.component.scss']
})
export class HeroComponent implements OnInit {
  constructor(@Inject(PLATFORM_ID) private platformId: Object) {}

  ngOnInit(): void {
    if (isPlatformBrowser(this.platformId)) {
      const heroFigure = document.querySelector('.hero-figure') as HTMLElement;
      const heroCopy = document.querySelector('.hero-copy') as HTMLElement;
      const typingText = document.getElementById('typing-text') as HTMLElement;

      const phrases = ["Boost Productivity", "Produce Creativity", "Enhance Efficiency"];
      let currentPhraseIndex = 0;
      let currentCharIndex = 0;
      let isDeleting = false;

      function type() {
        const currentPhrase = phrases[currentPhraseIndex];
        if (isDeleting) {
          currentCharIndex--;
        } else {
          currentCharIndex++;
        }

        typingText.textContent = currentPhrase.substring(0, currentCharIndex);

        if (!isDeleting && currentCharIndex === currentPhrase.length) {
          setTimeout(() => isDeleting = true, 1000);
        } else if (isDeleting && currentCharIndex === 0) {
          isDeleting = false;
          currentPhraseIndex = (currentPhraseIndex + 1) % phrases.length;
        }

        setTimeout(type, isDeleting ? 50 : 100);
      }

      type();

      function isInViewport(element: HTMLElement) {
        const rect = element.getBoundingClientRect();
        return (
          rect.top >= 0 &&
          rect.left >= 0 &&
          rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&
          rect.right <= (window.innerWidth || document.documentElement.clientWidth)
        );
      }

      function onScroll() {
        if (heroFigure) {
          if (isInViewport(heroFigure)) {
            heroFigure.classList.add('animate');
          } else {
            heroFigure.classList.remove('animate');
          }
        }

        if (heroCopy) {
          if (isInViewport(heroCopy)) {
            heroCopy.classList.add('in-view');
            heroCopy.classList.remove('out-of-view');
          } else {
            heroCopy.classList.add('out-of-view');
            heroCopy.classList.remove('in-view');
          }
        }
      }

      window.addEventListener('scroll', onScroll);
      onScroll(); // Check if already in viewport on load
    }
  }
}